#include "core/ConformanceAdapter.h"
#include "core/ConformanceWorker.h"
#include "ui/DiscoveryFormat.h"

extern "C" {
#include "leap/conformance/leap_conformance.h"
#include "leap/conformance/leap_conformance_export.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_protocol.h"
#include "leap_conformance_win_io.h"
}

#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>
#include <vector>

static ConformanceAdapter* g_progress_adapter = nullptr;

static QString formatPeerMac(const uint8_t mac[6]) {
    return QString::asprintf(
        "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

static DiscoveryPeerRow rowFromIdentify(const uint8_t mac[6],
                                        const LeapIdentifyReply& reply) {
    DiscoveryPeerRow row;
    row.mac = formatPeerMac(mac);
    row.productCode = reply.identity.product_code;
    row.platform =
        leap::studio::discovery::platformName(reply.identity.product_code);
    row.product = leap::studio::discovery::productName(reply.identity.product_code);
    row.profile = leap::studio::discovery::profileText(reply.active_profile_id);
    row.stateCode = reply.current_state;
    row.state = leap::studio::discovery::stateName(reply.current_state);
    row.leapVersion = leap::studio::discovery::leapProtocolText();
    row.fw = QString::number(reply.identity.firmware_revision);
    row.vendor = leap::studio::discovery::vendorName(
        reply.identity.vendor_id, reply.identity.product_code);
    return row;
}

static DiscoveryPeerRow rowFromPeerEntry(const LeapControllerPeerEntry& entry) {
    DiscoveryPeerRow row;
    row.mac = formatPeerMac(entry.mac);
    row.platform = QStringLiteral("—");
    row.product = QStringLiteral("—");
    row.profile = leap::studio::discovery::profileText(entry.active_profile_id);
    row.stateCode = entry.device_state;
    row.state = leap::studio::discovery::stateName(entry.device_state);
    row.leapVersion = leap::studio::discovery::leapProtocolText();
    row.fw = QStringLiteral("—");
    row.vendor = QStringLiteral("—");
    return row;
}

static void studio_progress(void* ctx, const LeapConformanceProgress* progress) {
    auto* adapter = static_cast<ConformanceAdapter*>(ctx);
    if (adapter == nullptr || progress == nullptr ||
        !adapter->conformanceRunInProgress()) {
        return;
    }

    if (progress->phase == LEAP_CONF_PROGRESS_START) {
        QMetaObject::invokeMethod(
            adapter,
            [adapter]() {
                if (!adapter->conformanceRunInProgress()) {
                    return;
                }
                emit adapter->logLine(QStringLiteral("--- conformance run start ---"));
                emit adapter->progressUpdated(QString(), 0);
            },
            Qt::QueuedConnection);
    }
    if (progress->phase == LEAP_CONF_PROGRESS_STEP && progress->step_name != nullptr) {
        QMetaObject::invokeMethod(
            adapter,
            [adapter, name = QString::fromUtf8(progress->step_name),
             percent = progress->percent]() {
                if (!adapter->conformanceRunInProgress()) {
                    return;
                }
                emit adapter->progressUpdated(name, percent);
            },
            Qt::QueuedConnection);
    }
    if (progress->phase == LEAP_CONF_PROGRESS_DONE) {
        QMetaObject::invokeMethod(
            adapter,
            [adapter]() {
                if (!adapter->conformanceRunInProgress()) {
                    return;
                }
                emit adapter->progressUpdated(QStringLiteral("Done"), 100);
            },
            Qt::QueuedConnection);
    }
    if (progress->phase == LEAP_CONF_PROGRESS_METRICS && progress->metrics != nullptr) {
        const LeapConformanceMetrics metrics = *progress->metrics;
        QMetaObject::invokeMethod(
            adapter,
            [adapter, metrics]() {
                if (!adapter->conformanceRunInProgress()) {
                    return;
                }
                emit adapter->soakMetricsUpdated(metrics);
            },
            Qt::QueuedConnection);
    }
    (void)g_progress_adapter;
}

ConformanceAdapter::ConformanceAdapter(QObject* parent) : QObject(parent) {
    g_progress_adapter = this;
    worker_ = new ConformanceWorker();
    worker_->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    workerThread_.start();
}

ConformanceAdapter::~ConformanceAdapter() {
    shutdown();
}

void ConformanceAdapter::shutdown() {
    if (shutdownDone_) {
        return;
    }
    shutdownDone_ = true;

    if (g_progress_adapter == this) {
        g_progress_adapter = nullptr;
    }

    cancelRun();
    monitorActive_ = false;
    conformanceRunActive_ = false;
    workerRunBusy_ = false;

    if (worker_ == nullptr || !workerThread_.isRunning()) {
        return;
    }

    QMetaObject::invokeMethod(
        worker_,
        [this]() {
            if (worker_->monitorTimer() != nullptr) {
                worker_->monitorTimer()->stop();
            }
            if (worker_->io() != nullptr) {
                worker_->io()->close_transport(worker_->io()->user_ctx);
            }
        },
        Qt::QueuedConnection);

    workerThread_.quit();
    if (!workerThread_.wait(3000)) {
        workerThread_.terminate();
        workerThread_.wait();
    }
}

void ConformanceAdapter::openAdapter(const QString& adapterPath,
                                     const QString& capturePcap) {
    QMetaObject::invokeMethod(
        worker_,
        [this, adapterPath, capturePcap]() {
            if (worker_->io() == nullptr) {
                emit logLine(QStringLiteral("[FAIL] conformance IO unavailable"));
                return;
            }
            if (adapterPath.isEmpty()) {
                emit logLine(QStringLiteral("[FAIL] no adapter selected"));
                return;
            }
            if (adapterPath_ == adapterPath &&
                leap_conformance_win_transport_is_open(worker_->ctx()) != 0) {
                return;
            }
            if (worker_->io()->open_transport(
                    worker_->io()->user_ctx,
                    adapterPath.toUtf8().constData(),
                    capturePcap.isEmpty() ? nullptr
                                          : capturePcap.toUtf8().constData()) != 0) {
                emit logLine(QStringLiteral("[FAIL] adapter open (run as Admin?)"));
                return;
            }
            adapterPath_ = adapterPath;
            emit logLine(QStringLiteral("Adapter: [Ok] %1").arg(adapterPath));
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::prepareIoSession(const QString& adapterPath,
                                          const QString& peerMac) {
    QMetaObject::invokeMethod(
        worker_,
        [this, adapterPath, peerMac]() {
            const QString path =
                adapterPath.isEmpty() ? adapterPath_ : adapterPath;
            uint8_t mac[6]{};

            if (path.isEmpty() || worker_->io() == nullptr) {
                emit ioSessionReady(false, QStringLiteral("Select a NIC first"));
                return;
            }
            if (peerMac.isEmpty() ||
                !leap_controller_peer_parse_mac(peerMac.toUtf8().constData(), mac)) {
                emit ioSessionReady(false, QStringLiteral("Set a valid peer MAC"));
                return;
            }
            if (worker_->io()->open_transport(
                    worker_->io()->user_ctx, path.toUtf8().constData(),
                    nullptr) != 0) {
                emit ioSessionReady(false, QStringLiteral("Adapter open failed"));
                return;
            }
            worker_->setPeerMac(mac, 1);
            adapterPath_ = path;

            if (leap_conformance_win_prepare_io_session(worker_->ctx()) != 0) {
                emit ioSessionReady(
                    false,
                    QStringLiteral("Bootstrap failed — check device on bench NIC"));
                return;
            }

            emit logLine(QStringLiteral("I/O session: [Ok] OP (ready for soak)"));
            emit ioSessionReady(true, QStringLiteral("Connected (OP)"));
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::setIoBenchDiagEnabled(bool enabled) {
    QMetaObject::invokeMethod(
        worker_,
        [this, enabled]() {
            leap_conformance_win_set_io_soak_diag_enabled(
                worker_->ctx(), enabled ? 1 : 0);
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::closeAdapter() {
    QMetaObject::invokeMethod(
        worker_,
        [this]() {
            if (worker_->io() != nullptr) {
                worker_->io()->close_transport(worker_->io()->user_ctx);
            }
            adapterPath_.clear();
            emit logLine(QStringLiteral("Adapter closed"));
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::runScenario(const QString& scenarioId,
                                     const QStringList& stepFilter,
                                     const QString& adapterPath,
                                     const QString& adapterLabel,
                                     const QString& peerMac,
                                     unsigned cyclicSeconds,
                                     unsigned cyclicPeriodMs) {
    const quint64 token = ++runToken_;
    QMetaObject::invokeMethod(
        worker_,
        [this, token, scenarioId, stepFilter, adapterPath, adapterLabel, peerMac,
         cyclicSeconds, cyclicPeriodMs]() {
            LeapConformanceRunConfig config{};
            LeapConformanceRunResult result{};
            std::vector<QByteArray> configStorage;
            std::vector<QByteArray> filterStorage;
            std::vector<const char*> filterPtrs;
            uint8_t mac[6]{};
            const QString path = adapterPath.isEmpty() ? adapterPath_ : adapterPath;

            const auto storeUtf8 = [&configStorage](const QString& text) -> const char* {
                configStorage.push_back(text.toUtf8());
                return configStorage.back().constData();
            };

            if (scenarioId == QStringLiteral("io_exchange_bench") &&
                leap_conformance_win_io_session_prepared(worker_->ctx()) == 0) {
                emit logLine(QStringLiteral("[FAIL] I/O bench requires Connect first"));
                emit runFinished(false, QStringLiteral("Connect I/O session first"), token);
                return;
            }

            lastRunStartedLocal_ =
                QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            lastNicName_ = adapterLabel;
            lastCyclicSeconds_ = cyclicSeconds;
            lastCyclicPeriodMs_ = cyclicPeriodMs;
            conformanceRunActive_ = true;
            workerRunBusy_ = true;

            if (!peerMac.isEmpty() &&
                leap_controller_peer_parse_mac(storeUtf8(peerMac), mac)) {
                worker_->setPeerMac(mac, 1);
                config.has_peer_mac = 1;
                memcpy(config.peer_mac, mac, 6);
                config.peer_mac_text = configStorage.back().constData();
            }

            for (const QString& step : stepFilter) {
                filterStorage.push_back(step.toUtf8());
                filterPtrs.push_back(filterStorage.back().constData());
            }

            config.scenario_id = storeUtf8(scenarioId);
            config.adapter = path.isEmpty() ? nullptr : storeUtf8(path);
            config.cyclic_seconds = cyclicSeconds;
            config.cyclic_period_ms = cyclicPeriodMs;
            config.step_filter = filterPtrs.empty() ? nullptr : filterPtrs.data();
            config.step_filter_count = filterPtrs.size();
            config.progress_fn = studio_progress;
            config.progress_ctx = this;
            config.io = worker_->io();
            config.keep_session_open = 1;

            leap_conformance_win_reset_latency_trend(worker_->ctx());
            leap_conformance_win_set_progress(
                worker_->ctx(), studio_progress, this);
            const LeapConformanceStatus status =
                leap_conformance_run_steps(&config, &result);
            leap_conformance_win_set_progress(worker_->ctx(), nullptr, nullptr);
            const bool cancelled = status == LEAP_CONF_CANCELLED;
            const bool pass =
                status == LEAP_CONF_OK && result.summary.failed == 0u;

            if (!path.isEmpty()) {
                adapterPath_ = path;
            }

            QStringList tableRows;
            for (size_t i = 0; i < result.step_count; ++i) {
                const auto& step = result.steps[i];
                const QString stepStatus =
                    QString::fromUtf8(leap_conformance_step_status_text(step.status));
                const QString phase = QString::fromUtf8(step.phase);
                const QString name = QString::fromUtf8(step.name);
                const QString detail = QString::fromUtf8(step.detail);
                emit logLine(
                    QStringLiteral("[%1] %2 — %3").arg(stepStatus, name, detail));
                tableRows.append(
                    phase + QLatin1Char('\t') + name + QLatin1Char('\t') + stepStatus +
                    QLatin1Char('\t') + detail);
            }
            emit conformanceRows(tableRows, token);

            lastResult_ = result;
            hasLastResult_ = result.step_count > 0u;

            conformanceRunActive_ = false;
            workerRunBusy_ = false;

            const QString summary =
                cancelled
                    ? (result.step_count > 0u
                           ? QStringLiteral("Cancelled (partial: Passed %1 Failed %2)")
                                 .arg(result.summary.passed)
                                 .arg(result.summary.failed)
                           : QStringLiteral("Cancelled"))
                    : QStringLiteral("Passed %1 Failed %2")
                          .arg(result.summary.passed)
                          .arg(result.summary.failed);

            if (cancelled) {
                emit logLine(QStringLiteral("Run stopped"));
            }

            emit runFinished(pass, summary, token);
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::exportReport(const QString& path,
                                      const DiscoveryPeerRow& device,
                                      bool hasDevice) {
    QMetaObject::invokeMethod(
        worker_,
        [this, path, device, hasDevice]() {
            if (path.isEmpty()) {
                emit exportFinished(false, path,
                                    QStringLiteral("No export path selected"));
                return;
            }
            if (!hasLastResult_) {
                emit exportFinished(
                    false, path,
                    QStringLiteral("Run conformance first — nothing to export"));
                return;
            }

            const QByteArray pathUtf = path.toUtf8();
            const QByteArray startedUtf = lastRunStartedLocal_.toUtf8();
            const QByteArray nicUtf = lastNicName_.toUtf8();
            const QByteArray toolUtf = QByteArray("leap_conformance_studio");
            const QByteArray deviceMacUtf = device.mac.toUtf8();
            const QByteArray devicePlatformUtf = device.platform.toUtf8();
            const QByteArray deviceProductUtf = device.product.toUtf8();
            const QByteArray deviceVendorUtf = device.vendor.toUtf8();
            const QByteArray deviceFwUtf = device.fw.toUtf8();
            const QByteArray leapProtocolUtf = device.leapVersion.toUtf8();
            LeapConformanceExportMeta meta{};

            meta.started_local  = startedUtf.constData();
            meta.nic_name       = nicUtf.isEmpty() ? nullptr : nicUtf.constData();
            meta.tool_version   = toolUtf.constData();
            meta.cyclic_seconds = lastCyclicSeconds_;
            meta.cyclic_period_ms = lastCyclicPeriodMs_;
            if (hasDevice) {
                meta.device_mac      = deviceMacUtf.constData();
                meta.device_platform = devicePlatformUtf.isEmpty()
                                           ? nullptr
                                           : devicePlatformUtf.constData();
                meta.device_product  = deviceProductUtf.isEmpty()
                                           ? nullptr
                                           : deviceProductUtf.constData();
                meta.device_vendor   = deviceVendorUtf.isEmpty()
                                           ? nullptr
                                           : deviceVendorUtf.constData();
                meta.device_fw       = deviceFwUtf.isEmpty() ? nullptr
                                                             : deviceFwUtf.constData();
                meta.leap_protocol   = leapProtocolUtf.isEmpty()
                                           ? nullptr
                                           : leapProtocolUtf.constData();
            }

            const QString suffix = QFileInfo(path).suffix().toLower();
            int rc = -1;
            if (suffix == QStringLiteral("csv")) {
                rc = leap_conformance_export_csv_path(pathUtf.constData(), &lastResult_);
            } else if (suffix == QStringLiteral("json")) {
                rc = leap_conformance_export_json_path(
                    pathUtf.constData(), &lastResult_, &meta);
            } else {
                rc = leap_conformance_export_markdown_path(
                    pathUtf.constData(), &lastResult_, &meta);
            }

            if (rc == 0) {
                emit exportFinished(true, path, QStringLiteral("Report written"));
                emit logLine(QStringLiteral("Export: [Ok] %1").arg(path));
            } else {
                emit exportFinished(
                    false, path,
                    QStringLiteral("Failed to write report (permissions/path?)"));
                emit logLine(QStringLiteral("Export: [Fail] %1").arg(path));
            }
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::cancelRun() {
    conformanceRunActive_ = false;
    if (worker_ == nullptr) {
        return;
    }
    if (worker_->ctx() != nullptr) {
        leap_conformance_win_set_progress(worker_->ctx(), nullptr, nullptr);
    }
    if (worker_->io() != nullptr) {
        leap_conformance_cancel(worker_->io());
    }
}

void ConformanceAdapter::discover(const QString& adapterPath, int scanMs) {
    QMetaObject::invokeMethod(
        worker_,
        [this, adapterPath, scanMs]() {
            unsigned peers = 0;
            QVector<DiscoveryPeerRow> discovered;

            if (worker_->io() == nullptr) {
                emit logLine(QStringLiteral("[FAIL] conformance IO unavailable"));
                emit discoveryPeers(discovered);
                return;
            }
            if (adapterPath.isEmpty()) {
                emit logLine(
                    QStringLiteral(
                        "[FAIL] no adapter selected — pick the bench NIC (e.g. Ethernet 3)"));
                emit discoveryPeers(discovered);
                return;
            }
            if (worker_->io()->open_transport(
                    worker_->io()->user_ctx,
                    adapterPath.toUtf8().constData(),
                    nullptr) != 0) {
                emit logLine(
                    QStringLiteral(
                        "[FAIL] adapter open on %1 (run Studio as Administrator?)")
                        .arg(adapterPath));
                emit discoveryPeers(discovered);
                return;
            }
            adapterPath_ = adapterPath;
            emit logLine(QStringLiteral("Adapter: [Ok] %1").arg(adapterPath));

            if (worker_->io()->discover_peers(worker_->io()->user_ctx, scanMs,
                                              &peers) != 0) {
                emit logLine(QStringLiteral("Discovery: [Fail] transport not ready"));
                emit discoveryPeers(discovered);
                return;
            }

            const LeapControllerPeerTable* table =
                leap_conformance_win_peer_table(worker_->ctx());
            if (table != nullptr) {
                for (unsigned i = 0; i < table->count; ++i) {
                    const LeapControllerPeerEntry* entry =
                        leap_controller_peer_table_get(table, i);
                    if (entry == nullptr) {
                        continue;
                    }

                    LeapIdentifyReply identify{};
                    DiscoveryPeerRow row;
                    if (leap_conformance_win_query_identify(
                            worker_->ctx(), entry->mac, &identify) == 0) {
                        row = rowFromIdentify(entry->mac, identify);
                    } else {
                        row = rowFromPeerEntry(*entry);
                    }
                    discovered.append(row);
                }
            }

            if (peers == 0) {
                emit logLine(
                    QStringLiteral(
                        "Discovery: [Ok] peers=0 on %1 — check bench NIC, cable, ESP32 power, Admin")
                        .arg(adapterPath));
            } else {
                emit logLine(
                    QStringLiteral("Discovery: [Ok] peers=%1 (IDENTIFY enriched)")
                        .arg(peers));
            }
            emit discoveryPeers(discovered);
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::identifyPeer(const QString& adapterPath,
                                      const QString& peerMac) {
    QMetaObject::invokeMethod(
        worker_,
        [this, adapterPath, peerMac]() {
            uint8_t mac[6]{};
            int ok = 0;
            const QString path = adapterPath.isEmpty() ? adapterPath_ : adapterPath;

            if (!leap_controller_peer_parse_mac(peerMac.toUtf8().constData(), mac)) {
                emit logLine(QStringLiteral("Identify: [Fail] bad MAC"));
                return;
            }
            if (worker_->io() == nullptr) {
                emit logLine(QStringLiteral("Identify: [Fail] IO unavailable"));
                return;
            }
            if (!path.isEmpty() &&
                worker_->io()->open_transport(
                    worker_->io()->user_ctx, path.toUtf8().constData(), nullptr) != 0) {
                emit logLine(QStringLiteral("Identify: [Fail] adapter open"));
                return;
            }
            if (worker_->io()->identify(worker_->io()->user_ctx, mac, &ok) == 0 &&
                ok) {
                emit logLine(QStringLiteral("Identify: [Ok]"));
            } else {
                emit logLine(QStringLiteral("Identify: [Timeout]"));
            }
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::locatePeer(const QString& adapterPath,
                                    const QString& peerMac,
                                    unsigned durationMs) {
    QMetaObject::invokeMethod(
        worker_,
        [this, adapterPath, peerMac, durationMs]() {
            uint8_t mac[6]{};
            int ok = 0;
            const QString path = adapterPath.isEmpty() ? adapterPath_ : adapterPath;

            if (!leap_controller_peer_parse_mac(peerMac.toUtf8().constData(), mac)) {
                emit logLine(QStringLiteral("Locate: [Fail] bad MAC"));
                return;
            }
            if (worker_->io() == nullptr) {
                emit logLine(QStringLiteral("Locate: [Fail] IO unavailable"));
                return;
            }
            if (!path.isEmpty() &&
                worker_->io()->open_transport(
                    worker_->io()->user_ctx, path.toUtf8().constData(), nullptr) != 0) {
                emit logLine(QStringLiteral("Locate: [Fail] adapter open"));
                return;
            }
            if (worker_->io()->locate(worker_->io()->user_ctx, mac, durationMs,
                                      &ok) == 0 &&
                ok) {
                emit logLine(
                    QStringLiteral("Locate: [Ok] duration_ms=%1").arg(durationMs));
            } else {
                emit logLine(QStringLiteral("Locate: [Timeout]"));
            }
        },
        Qt::QueuedConnection);
}

static void studio_poll_metrics(ConformanceAdapter* adapter, ConformanceWorker* worker,
                                bool logResult) {
    if (adapter == nullptr || worker == nullptr || worker->io() == nullptr) {
        return;
    }

    LeapConformanceMetrics metrics{};
    if (leap_conformance_win_transport_is_open(worker->ctx()) == 0) {
        if (logResult) {
            emit adapter->logLine(
                QStringLiteral("Snapshot: [Fail] open adapter first"));
        }
        return;
    }

    if (leap_conformance_win_prepare_diagnostics(worker->ctx()) != 0) {
        if (logResult) {
            emit adapter->logLine(
                QStringLiteral(
                    "Snapshot: [Fail] no peer session — check bench NIC and peer MAC"));
        }
        return;
    }

    leap_conformance_win_invalidate_diag_cache(worker->ctx());
    if (worker->io()->snapshot(worker->io()->user_ctx, &metrics) != 0) {
        if (logResult) {
            emit adapter->logLine(QStringLiteral("Snapshot: [Fail] transport read"));
        }
        return;
    }

    QMetaObject::invokeMethod(
        adapter,
        [adapter, metrics]() { emit adapter->metricsUpdated(metrics); },
        Qt::QueuedConnection);

    if (logResult) {
        const QString owner =
            metrics.has_session_owner != 0
                ? formatPeerMac(metrics.session_owner_mac)
                : QStringLiteral("none");
        const QString frameSource =
            metrics.frames_from_device != 0 ? QStringLiteral("device")
                                            : QStringLiteral("NIC");
        const QString cycleHint =
            metrics.pd.cycles_completed > 0u
                ? QStringLiteral(" avg_cycle=%1 us")
                      .arg(metrics.pd.total_cycle_period_us /
                           metrics.pd.cycles_completed)
                : QString();
        emit adapter->logLine(
            QStringLiteral(
                "Snapshot: [Ok] state=0x%1 owner=%2 rx=%3 (%4) lease=%5 ms%6")
                .arg(metrics.device_state, 4, 16, QChar('0'))
                .arg(owner)
                .arg(metrics.rx_frames)
                .arg(frameSource)
                .arg(metrics.lease_remaining_us / 1000u)
                .arg(cycleHint));
    }
}

void ConformanceAdapter::startMonitor(unsigned intervalMs) {
    monitorActive_ = true;
    const unsigned periodMs = intervalMs > 0 ? intervalMs : 100u;

    QMetaObject::invokeMethod(
        worker_,
        [this, periodMs]() {
            if (worker_->monitorTimer() == nullptr) {
                auto* timer = new QTimer(worker_);
                worker_->setMonitorTimer(timer);
                connect(timer, &QTimer::timeout, worker_, [this]() {
                    if (!monitorActive_) {
                        return;
                    }
                    studio_poll_metrics(this, worker_, false);
                });
            }

            worker_->monitorTimer()->start(static_cast<int>(periodMs));
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::stopMonitor() {
    monitorActive_ = false;
    QMetaObject::invokeMethod(
        worker_,
        [this]() {
            if (worker_->monitorTimer() != nullptr) {
                worker_->monitorTimer()->stop();
            }
        },
        Qt::QueuedConnection);
}

void ConformanceAdapter::refreshSnapshot() {
    QMetaObject::invokeMethod(
        worker_,
        [this]() {
            if (worker_->io() == nullptr) {
                emit logLine(QStringLiteral("Snapshot: [Fail] IO unavailable"));
                return;
            }
            studio_poll_metrics(this, worker_, true);
        },
        Qt::QueuedConnection);
}
