#include "ui/DiagnosticsFormat.h"

#include "ui/DiscoveryFormat.h"

#include <QVector>
#include <cstdint>



extern "C" {

#include "leap/leap_controller_stack.h"

#include "leap/leap_protocol.h"

}



namespace leap::studio::diagnostics {



namespace {



QString withSource(const QString& text, const QString& source) {

    if (text == QStringLiteral("—") || source.isEmpty()) {

        return text;

    }

    return QStringLiteral("%1 (%2)").arg(text, source);

}



QString deviceStateText(const LeapConformanceMetrics& metrics) {

    if (metrics.device_state != 0u) {

        return leap::studio::discovery::stateName(metrics.device_state);

    }

    if (metrics.stack_phase == LEAP_CTRL_STACK_OP) {

        return QStringLiteral("OP");

    }

    if (metrics.stack_phase != 0u) {

        return QStringLiteral("stack %1").arg(metrics.stack_phase);

    }

    return QStringLiteral("—");

}



QString timingValue(uint32_t deviceUs, uint64_t controllerUs, bool hasDeviceTiming,

                    bool hasControllerTiming, const QString& sourceDevice,

                    const QString& sourceController) {

    if (hasDeviceTiming && deviceUs > 0u) {

        return withSource(formatDurationUs(deviceUs), sourceDevice);

    }

    if (hasControllerTiming && controllerUs > 0u) {

        return withSource(formatDurationUs64(controllerUs), sourceController);

    }

    return QStringLiteral("—");

}



QString bestCycleValue(uint32_t deviceMinUs, uint64_t controllerMinUs,

                       bool hasDeviceTiming, bool hasControllerTiming,

                       const QString& sourceDevice, const QString& sourceController) {

    if (hasDeviceTiming && deviceMinUs > 0u) {

        return withSource(formatDurationUs(deviceMinUs), sourceDevice);

    }

    if (hasControllerTiming && controllerMinUs > 0u) {

        return withSource(formatDurationUs64(controllerMinUs), sourceController);

    }

    return QStringLiteral("—");

}



struct TrendLatencyStats {
    bool hasData = false;
    uint32_t lastUs = 0u;
    uint32_t minUs = 0u;
    uint32_t maxUs = 0u;
    uint64_t avgUs = 0u;
};

TrendLatencyStats trendLatencyStats(const LeapConformanceMetrics& metrics) {
    TrendLatencyStats stats;

    if (metrics.reply_latency_trend.count == 0u) {
        return stats;
    }

    stats.hasData = true;
    stats.minUs = UINT32_MAX;
    uint64_t sum = 0u;

    for (uint32_t i = 0u; i < metrics.reply_latency_trend.count; i++) {
        const uint32_t sample = metrics.reply_latency_trend.samples[i];
        stats.lastUs = sample;
        if (sample < stats.minUs) {
            stats.minUs = sample;
        }
        if (sample > stats.maxUs) {
            stats.maxUs = sample;
        }
        sum += sample;
    }

    if (stats.minUs == UINT32_MAX) {
        stats.hasData = false;
        return stats;
    }

    stats.avgUs = sum / metrics.reply_latency_trend.count;
    return stats;
}

QString bestReplyLatencyValue(const LeapConformanceMetrics& metrics) {
    const TrendLatencyStats trend = trendLatencyStats(metrics);

    if (!trend.hasData) {
        return QStringLiteral("—");
    }

    return withSource(QStringLiteral("%1 µs").arg(trend.minUs),
                      QStringLiteral("controller"));
}

QString replyLatencyTrendValue(const QString& primary,
                               const TrendLatencyStats& trend,
                               uint32_t trendUs) {
    if (primary != QStringLiteral("—")) {
        return primary;
    }

    if (!trend.hasData || trendUs == 0u) {
        return QStringLiteral("—");
    }

    return withSource(QStringLiteral("%1 µs").arg(trendUs),
                      QStringLiteral("controller"));
}



}  // namespace



QString formatMac(const uint8_t mac[6]) {

    if (mac == nullptr) {

        return QStringLiteral("—");

    }



    return QString::asprintf(

        "%02x:%02x:%02x:%02x:%02x:%02x",

        mac[0],

        mac[1],

        mac[2],

        mac[3],

        mac[4],

        mac[5]);

}



QString formatDurationUs(uint32_t micros) {

    if (micros >= 1000000u) {

        return QStringLiteral("%1 s").arg(micros / 1000000.0, 0, 'f', 1);

    }

    if (micros >= 1000u) {

        return QStringLiteral("%1 ms").arg(micros / 1000.0, 0, 'f', 1);

    }

    return QStringLiteral("%1 µs").arg(micros);

}



QString formatDurationUs64(uint64_t micros) {

    if (micros >= 1000000ull) {

        return QStringLiteral("%1 s").arg(micros / 1000000.0, 0, 'f', 1);

    }

    if (micros >= 1000ull) {

        return QStringLiteral("%1 ms").arg(micros / 1000.0, 0, 'f', 1);

    }

    return QStringLiteral("%1 µs").arg(micros);

}



QString formatFrameCount(uint64_t count, int fromDevice) {

    const QString countText = QString::number(count);

    if (fromDevice != 0) {

        return QStringLiteral("%1 (device)").arg(countText);

    }

    return QStringLiteral("%1 (NIC)").arg(countText);

}



namespace {

DiagnosticsTableRow sectionRow(const QString& title) {
    DiagnosticsTableRow row;
    row.kind = DiagnosticsTableRow::Kind::Section;
    row.label = title;
    return row;
}

DiagnosticsTableRow fieldRow(const QString& label, const QString& value) {
    DiagnosticsTableRow row;
    row.kind = DiagnosticsTableRow::Kind::Field;
    row.label = label;
    row.value = value;
    return row;
}

}  // namespace

QString formatFrameRate(double fps, bool hasRate) {
    if (!hasRate || fps < 0.0) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1 fps").arg(fps, 0, 'f', 1);
}

void populateDiagnosticsRows(const LeapConformanceMetrics& metrics,
                             QVector<DiagnosticsTableRow>* rows,
                             const DiagnosticsTrafficRates& rates) {
    if (rows == nullptr) {
        return;
    }

    rows->clear();



    const bool hasDeviceTiming = metrics.has_cycle_timing != 0;

    const bool hasControllerTiming = metrics.pd.cycles_completed > 0u;



    const QString sessionOwner =

        metrics.has_session_owner != 0 ? formatMac(metrics.session_owner_mac)

                                     : QStringLiteral("none");



    const QString leaseRemaining =

        metrics.has_lease_watchdog != 0

            ? formatDurationUs(metrics.lease_remaining_us)

            : QStringLiteral("—");



    const QString watchdogRemaining =

        metrics.has_lease_watchdog != 0

            ? formatDurationUs(metrics.watchdog_remaining_us)

            : QStringLiteral("—");



    const QString bestCycle = bestCycleValue(

        metrics.timing.min_cycle_time_us,

        metrics.pd.min_cycle_period_us,

        hasDeviceTiming,

        hasControllerTiming && metrics.pd.min_cycle_period_us > 0u,

        QStringLiteral("device"),

        QStringLiteral("controller"));



    const QString lastCycle = timingValue(

        metrics.timing.last_cycle_time_us,

        metrics.pd.last_cycle_period_us,

        hasDeviceTiming,

        hasControllerTiming,

        QStringLiteral("device"),

        QStringLiteral("controller"));



    const QString worstCycle = timingValue(

        metrics.timing.max_cycle_time_us,

        metrics.pd.max_cycle_period_us,

        hasDeviceTiming,

        hasControllerTiming,

        QStringLiteral("device"),

        QStringLiteral("controller"));



    const TrendLatencyStats trendLatency = trendLatencyStats(metrics);



    QString avgCycle = QStringLiteral("—");

    if (hasControllerTiming) {

        const uint64_t avgUs =

            metrics.pd.total_cycle_period_us / metrics.pd.cycles_completed;

        avgCycle = withSource(formatDurationUs64(avgUs), QStringLiteral("controller"));

    }

    const QString lastReplyLatency = replyLatencyTrendValue(

        timingValue(

            metrics.timing.last_reply_latency_us,

            metrics.pd.last_latency_us,

            hasDeviceTiming,

            metrics.pd.pd_sent_ok > 0u || metrics.pd.exchange_replies > 0u,

            QStringLiteral("device"),

            QStringLiteral("controller")),

        trendLatency,

        trendLatency.lastUs);



    const QString bestReplyLatency = bestReplyLatencyValue(metrics);



    const QString worstReplyLatency = replyLatencyTrendValue(

        timingValue(

            metrics.timing.max_reply_latency_us,

            metrics.pd.max_latency_us,

            hasDeviceTiming,

            hasControllerTiming || metrics.pd.max_latency_us > 0u,

            QStringLiteral("device"),

            QStringLiteral("controller")),

        trendLatency,

        trendLatency.maxUs);



    const int frameSource = metrics.frames_from_device;



    *rows << sectionRow(QStringLiteral("Connection"))
          << fieldRow(QStringLiteral("Device State"), deviceStateText(metrics))
          << fieldRow(QStringLiteral("Session Owner"), sessionOwner)
          << fieldRow(QStringLiteral("Lease"), leaseRemaining)
          << fieldRow(QStringLiteral("Watchdog"), watchdogRemaining)
          << sectionRow(QStringLiteral("Traffic"))
          << fieldRow(QStringLiteral("RX Frames"),
                       formatFrameCount(metrics.rx_frames, frameSource))
          << fieldRow(QStringLiteral("TX Frames"),
                       formatFrameCount(metrics.tx_frames, frameSource))
          << fieldRow(QStringLiteral("RX Rate"),
                       formatFrameRate(rates.rxFps, rates.hasRates))
          << fieldRow(QStringLiteral("TX Rate"),
                       formatFrameRate(rates.txFps, rates.hasRates))
          << fieldRow(QStringLiteral("Duplicates"),
                       formatFrameCount(metrics.duplicate_frames, frameSource))
          << fieldRow(QStringLiteral("Stale"),
                       formatFrameCount(metrics.stale_frames, frameSource))
          << sectionRow(QStringLiteral("Timing"))
          << fieldRow(QStringLiteral("Best Cycle"), bestCycle)
          << fieldRow(QStringLiteral("Avg Cycle"), avgCycle)
          << fieldRow(QStringLiteral("Worst Cycle"), worstCycle)
          << fieldRow(QStringLiteral("Best Latency"), bestReplyLatency)
          << fieldRow(QStringLiteral("Last Latency"), lastReplyLatency)
          << fieldRow(QStringLiteral("Worst Latency"), worstReplyLatency);
}



}  // namespace leap::studio::diagnostics


