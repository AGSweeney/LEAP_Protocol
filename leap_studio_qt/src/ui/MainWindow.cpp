#include "ui/MainWindow.h"

extern "C" {
#include "leap/conformance/leap_conformance_metrics.h"
#include "leap/leap_log.h"
#include "leap/leap_raw_winpcap.h"
}

#include "core/ConformanceAdapter.h"
#include "ui/DiagnosticsFormat.h"
#include "ui/DiagnosticsLatencyChart.h"
#include "ui/DiscoveryFormat.h"
#include "ui/theme/StatusPalette.h"
#include "ui/theme/ThemeManager.h"

extern "C" {
#include "leap/leap_controller_stack.h"
#include "leap/leap_protocol.h"
}
#include "ui/theme/ThemeTokens.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QAbstractItemView>
#include <QBrush>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cstring>

namespace {

QPushButton* makeButton(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setObjectName(QStringLiteral("ToolbarButton"));
    return btn;
}

}  // namespace

void MainWindow::leapLogSink(void* ctx, int is_error, const char* line) {
    auto* window = static_cast<MainWindow*>(ctx);
    if (window == nullptr || line == nullptr) {
        return;
    }

    QString text = QString::fromUtf8(line).trimmed();
    if (text.isEmpty()) {
        return;
    }
    if (is_error != 0) {
        text = QStringLiteral("[stderr] ") + text;
    }

    QMetaObject::invokeMethod(
        window,
        [window, text]() { window->appendLog(text); },
        Qt::QueuedConnection);
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName(QStringLiteral("MainWindow"));
    resize(1280, 820);
    setWindowTitle(QStringLiteral("LEAP Conformance Studio"));

    leap_log_reset_origin();
    leap_log_set_sink(&MainWindow::leapLogSink, this);

    adapter_ = new ConformanceAdapter(this);
    connect(adapter_, &ConformanceAdapter::logLine, this,
            [this](const QString& line) {
                appendLog(line);
                if (connectionStatus_ != nullptr &&
                    line.startsWith(QStringLiteral("Adapter: [Ok]"))) {
                    connectionStatus_->setText(line);
                }
            });
    connect(adapter_, &ConformanceAdapter::discoveryPeers, this,
            &MainWindow::onDiscoveryPeers);
    connect(adapter_, &ConformanceAdapter::conformanceRows, this,
            &MainWindow::onConformanceRows);
    connect(adapter_, &ConformanceAdapter::progressUpdated, this,
            &MainWindow::onProgressUpdated);
    connect(adapter_, &ConformanceAdapter::runFinished, this,
            &MainWindow::onRunFinished);
    connect(adapter_, &ConformanceAdapter::exportFinished, this,
            [this](bool ok, const QString& path, const QString& detail) {
                appendLog(QStringLiteral("Export %1: %2 — %3")
                              .arg(ok ? QStringLiteral("[Ok]")
                                      : QStringLiteral("[Fail]"),
                                      path, detail));
                setStatusText(detail);
            });
    connect(adapter_, &ConformanceAdapter::metricsUpdated, this,
            [this](const LeapConformanceMetrics& metrics) {
                if (metricsTable_ != nullptr) {
                    metricsTable_->setRowCount(8);
                    const auto set = [this](int row, const QString& key,
                                            const QString& val) {
                        metricsTable_->setItem(row, 0, new QTableWidgetItem(key));
                        metricsTable_->setItem(row, 1, new QTableWidgetItem(val));
                    };
                    set(0, QStringLiteral("pd_ok"),
                        QString::number(metrics.pd.pd_sent_ok));
                    set(1, QStringLiteral("pd_fail"),
                        QString::number(metrics.pd.pd_sent_fail));
                    set(2, QStringLiteral("heartbeats"),
                        QString::number(metrics.pd.heartbeats_sent));
                    set(3, QStringLiteral("cycles"),
                        QString::number(metrics.pd.cycles_completed));
                    set(4, QStringLiteral("link_up"),
                        metrics.link.link_up ? QStringLiteral("yes")
                                             : QStringLiteral("no"));
                    set(5, QStringLiteral("iface_up"),
                        metrics.link.interface_up ? QStringLiteral("yes")
                                                  : QStringLiteral("no"));
                    set(6, QStringLiteral("stack_phase"),
                        QString::number(metrics.stack_phase));
                    set(7, QStringLiteral("rx_frames"),
                        QString::number(metrics.transport.rx_frames_ok));
                }
                populateDiagnosticsTable(metrics);
            });

    lastSeenTimer_ = new QTimer(this);
    lastSeenTimer_->setInterval(1000);
    connect(lastSeenTimer_, &QTimer::timeout, this,
            &MainWindow::refreshDiscoveryLastSeen);
    lastSeenTimer_->start();

    buildUi();
    applySavedTheme();
    if (diagnosticsLatencyChart_ != nullptr) {
        diagnosticsLatencyChart_->refreshTheme();
    }
    refreshAdapterList();
    setStatusText(QStringLiteral("Ready"));
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    mainTabs_ = new QTabWidget(central);
    mainTabs_->setObjectName(QStringLiteral("StudioTabs"));
    auto* connectionPage = new QWidget(mainTabs_);
    auto* discoveryPage = new QWidget(mainTabs_);
    auto* conformancePage = new QWidget(mainTabs_);
    auto* monitorPage = new QWidget(mainTabs_);
    auto* diagnosticsPage = new QWidget(mainTabs_);
    buildConnectionTab(connectionPage);
    buildDiscoveryTab(discoveryPage);
    buildConformanceTab(conformancePage);
    buildMonitorTab(monitorPage);
    buildDiagnosticsTab(diagnosticsPage);
    mainTabs_->addTab(connectionPage, QStringLiteral("Connection"));
    mainTabs_->addTab(discoveryPage, QStringLiteral("Discovery"));
    mainTabs_->addTab(conformancePage, QStringLiteral("Conformance"));
    mainTabs_->addTab(monitorPage, QStringLiteral("Monitor"));
    mainTabs_->addTab(diagnosticsPage, QStringLiteral("Diagnostics"));
    auto* logGroup = new QGroupBox(QStringLiteral("Activity log"), central);
    logGroup->setCheckable(true);
    logGroup->setChecked(true);
    auto* logLayout = new QVBoxLayout(logGroup);
    log_ = new QPlainTextEdit(logGroup);
    log_->setObjectName(QStringLiteral("OutputLog"));
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setMinimumHeight(72);
    logLayout->addWidget(log_);

    mainVerticalSplitter_ = new QSplitter(Qt::Vertical, central);
    mainVerticalSplitter_->setObjectName(QStringLiteral("MainVerticalSplitter"));
    mainVerticalSplitter_->addWidget(mainTabs_);
    mainVerticalSplitter_->addWidget(logGroup);
    mainVerticalSplitter_->setStretchFactor(0, 7);
    mainVerticalSplitter_->setStretchFactor(1, 3);
    mainVerticalSplitter_->setChildrenCollapsible(true);
    root->addWidget(mainVerticalSplitter_, 1);

    connect(mainVerticalSplitter_, &QSplitter::splitterMoved, this,
            [this](int, int) { saveMainSplitter(); });

    QTimer::singleShot(0, this, [this]() { restoreMainSplitter(); });

    connect(logGroup, &QGroupBox::toggled, logGroup, [logGroup](bool checked) {
        if (logGroup->layout() != nullptr && logGroup->layout()->count() > 0) {
            if (auto* child = logGroup->layout()->itemAt(0)->widget()) {
                child->setVisible(checked);
            }
        }
    });

    setCentralWidget(central);

    auto* themeDark = makeButton(QStringLiteral("Dark"), this);
    auto* themeLight = makeButton(QStringLiteral("Light"), this);
    connect(themeDark, &QPushButton::clicked, this, &MainWindow::setThemeDark);
    connect(themeLight, &QPushButton::clicked, this, &MainWindow::setThemeLight);
    statusBar()->addPermanentWidget(themeDark);
    statusBar()->addPermanentWidget(themeLight);
}

void MainWindow::buildConnectionTab(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    auto* formGroup = new QGroupBox(QStringLiteral("Bench connection"), page);
    auto* form = new QFormLayout(formGroup);
    adapterCombo_ = new QComboBox(formGroup);
    adapterCombo_->setObjectName(QStringLiteral("AdapterCombo"));
    adapterCombo_->setMinimumWidth(480);
    leap::studio::theme::ThemeManager::styleComboBoxPopup(adapterCombo_);
    peerMacEdit_ = new QLineEdit(formGroup);
    peerMacEdit_->setPlaceholderText(
        QStringLiteral("Set from Discovery tab after scan (column MAC)"));
    form->addRow(QStringLiteral("Npcap adapter"), adapterCombo_);
    form->addRow(QStringLiteral("Expected peer MAC"), peerMacEdit_);
    layout->addWidget(formGroup);

    auto* actions = new QHBoxLayout();
    auto* refreshBtn = makeButton(QStringLiteral("Refresh adapters"), page);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onListAdapters);
    actions->addWidget(refreshBtn);
    auto* openBtn = makeButton(QStringLiteral("Open adapter"), page);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenAdapter);
    actions->addWidget(openBtn);
    auto* closeBtn = makeButton(QStringLiteral("Close adapter"), page);
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::onCloseAdapter);
    actions->addWidget(closeBtn);
    actions->addStretch();
    layout->addLayout(actions);

    connectionStatus_ = new QLabel(
        QStringLiteral("No adapter opened. Run as Administrator for Npcap capture."),
        page);
    connectionStatus_->setWordWrap(true);
    layout->addWidget(connectionStatus_);
    layout->addStretch();
}

void MainWindow::buildDiscoveryTab(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    auto* actions = new QHBoxLayout();
    auto* discoverBtn = makeButton(QStringLiteral("Scan for peers (3s)"), page);
    connect(discoverBtn, &QPushButton::clicked, this, &MainWindow::onDiscover);
    actions->addWidget(discoverBtn);
    auto* identifyBtn = makeButton(QStringLiteral("Identify peer"), page);
    connect(identifyBtn, &QPushButton::clicked, this, &MainWindow::onIdentify);
    actions->addWidget(identifyBtn);
    auto* locateBtn = makeButton(QStringLiteral("Locate peer"), page);
    connect(locateBtn, &QPushButton::clicked, this, &MainWindow::onLocate);
    actions->addWidget(locateBtn);
    actions->addStretch();
    layout->addLayout(actions);

    discoveryTable_ = new QTableWidget(0, 9, page);
    discoveryTable_->setHorizontalHeaderLabels(
        {QStringLiteral("MAC"), QStringLiteral("Platform"), QStringLiteral("Product"),
         QStringLiteral("Profile"), QStringLiteral("State"), QStringLiteral("LEAP"),
         QStringLiteral("FW"), QStringLiteral("Vendor"),
         QStringLiteral("Last Conformance")});
    discoveryTable_->setAlternatingRowColors(true);
    discoveryTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    discoveryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    discoveryTable_->verticalHeader()->setVisible(false);
    discoveryTable_->horizontalHeader()->setStretchLastSection(true);
    discoveryTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    discoveryTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(discoveryTable_);
}

void MainWindow::buildConformanceTab(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    auto* runRow = new QHBoxLayout();
    auto* runAllBtn = makeButton(QStringLiteral("Run all steps"), page);
    connect(runAllBtn, &QPushButton::clicked, this, &MainWindow::onRunAll);
    runRow->addWidget(runAllBtn);
    auto* runSelBtn = makeButton(QStringLiteral("Run selected"), page);
    connect(runSelBtn, &QPushButton::clicked, this, &MainWindow::onRunSelected);
    runRow->addWidget(runSelBtn);
    auto* stopBtn = makeButton(QStringLiteral("Stop"), page);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    runRow->addWidget(stopBtn);
    auto* exportBtn = makeButton(QStringLiteral("Export report"), page);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExportReport);
    runRow->addWidget(exportBtn);
    runRow->addStretch();
    layout->addLayout(runRow);

    runProgress_ = new QProgressBar(page);
    runProgress_->setRange(0, 100);
    runProgress_->setValue(0);
    layout->addWidget(runProgress_);

    runSummary_ = new QLabel(QStringLiteral("No conformance run yet."), page);
    layout->addWidget(runSummary_);

    auto* stepsGroup = new QGroupBox(QStringLiteral("Step filter"), page);
    auto* stepsLayout = new QHBoxLayout(stepsGroup);
    for (const QString& step :
         {QStringLiteral("discover"), QStringLiteral("probe_caps"),
          QStringLiteral("bootstrap_pd"), QStringLiteral("diag"),
          QStringLiteral("cyclic_write"),
          QStringLiteral("cyclic_exch"), QStringLiteral("pd_masks"),
          QStringLiteral("identify"), QStringLiteral("locate")}) {
        auto* cb = new QCheckBox(step, stepsGroup);
        cb->setChecked(true);
        stepChecks_.append(cb);
        stepsLayout->addWidget(cb);
    }
    stepsLayout->addStretch();
    layout->addWidget(stepsGroup);

    // Cyclic PD period control (exposed so user can change the target interval
    // used for the "cyclic_write" and "cyclic_exch" steps, instead of the
    // previous hard-coded 100 ms).
    auto* cyclicRow = new QHBoxLayout();
    auto* cyclicLabel = new QLabel(QStringLiteral("PD cycle period (ms):"), page);
    cyclePeriodSpin_ = new QSpinBox(page);
    cyclePeriodSpin_->setRange(0, 2000);
    cyclePeriodSpin_->setSingleStep(10);
    cyclePeriodSpin_->setSpecialValueText(QStringLiteral("freerun"));
    cyclePeriodSpin_->setValue(100);
    cyclePeriodSpin_->setSuffix(QStringLiteral(" ms"));
    cyclePeriodSpin_->setToolTip(
        QStringLiteral("Target PD cycle period. 0 = freerun (no delay between cycles)."));
    cyclicRow->addWidget(cyclicLabel);
    cyclicRow->addWidget(cyclePeriodSpin_);
    cyclicRow->addStretch();
    layout->addLayout(cyclicRow);

    resultsTable_ = new QTableWidget(0, 4, page);
    resultsTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Phase"), QStringLiteral("Test"), QStringLiteral("Status"),
         QStringLiteral("Detail")});
    resultsTable_->horizontalHeader()->setStretchLastSection(true);
    resultsTable_->setAlternatingRowColors(true);
    resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(resultsTable_, 1);
}

void MainWindow::buildMonitorTab(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    auto* actions = new QHBoxLayout();
    auto* startBtn = makeButton(QStringLiteral("Start live monitor"), page);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::onStartMonitor);
    actions->addWidget(startBtn);
    auto* stopBtn = makeButton(QStringLiteral("Stop monitor"), page);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStopMonitor);
    actions->addWidget(stopBtn);
    auto* snapBtn = makeButton(QStringLiteral("Snapshot now"), page);
    connect(snapBtn, &QPushButton::clicked, this, &MainWindow::onRefreshSnapshot);
    actions->addWidget(snapBtn);
    actions->addStretch();
    layout->addLayout(actions);

    metricsTable_ = new QTableWidget(0, 2, page);
    metricsTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Metric"), QStringLiteral("Value")});
    metricsTable_->horizontalHeader()->setStretchLastSection(true);
    metricsTable_->setAlternatingRowColors(true);
    metricsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(metricsTable_, 1);
}

void MainWindow::buildDiagnosticsTab(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* actions = new QHBoxLayout();
    auto* startBtn = makeButton(QStringLiteral("Start live diagnostics"), page);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::onStartDiagnostics);
    actions->addWidget(startBtn);
    auto* stopBtn = makeButton(QStringLiteral("Stop"), page);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStopDiagnostics);
    actions->addWidget(stopBtn);
    auto* refreshBtn = makeButton(QStringLiteral("Refresh now"), page);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshDiagnostics);
    actions->addWidget(refreshBtn);
    actions->addStretch();
    layout->addLayout(actions);

    constexpr int kDiagSectionCount = 3;
    constexpr int kDiagFieldCount = 17;
    constexpr int kDiagSectionRowHeight = 22;
    constexpr int kDiagRowHeight = 24;
    constexpr int kDiagRowCount = kDiagSectionCount + kDiagFieldCount;
    constexpr int kDiagFieldColumnWidth = 168;
    constexpr int kDiagTableWidth = 372;
    constexpr int kDiagTableBodyHeight =
        kDiagSectionCount * kDiagSectionRowHeight + kDiagFieldCount * kDiagRowHeight;

    diagnosticsTable_ = new QTableWidget(kDiagRowCount, 2, page);
    diagnosticsTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Field"), QStringLiteral("Value")});
    diagnosticsTable_->verticalHeader()->setVisible(false);
    diagnosticsTable_->verticalHeader()->setDefaultSectionSize(kDiagRowHeight);
    diagnosticsTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    diagnosticsTable_->horizontalHeader()->setStretchLastSection(false);
    diagnosticsTable_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Fixed);
    diagnosticsTable_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    diagnosticsTable_->horizontalHeader()->resizeSection(0, kDiagFieldColumnWidth);
    diagnosticsTable_->setColumnWidth(0, kDiagFieldColumnWidth);
    diagnosticsTable_->setFixedWidth(kDiagTableWidth);
    diagnosticsTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    diagnosticsTable_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    diagnosticsTable_->setFixedHeight(
        diagnosticsTable_->horizontalHeader()->height() + kDiagTableBodyHeight + 2);
    diagnosticsTable_->setAlternatingRowColors(true);
    diagnosticsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    diagnosticsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    diagnosticsTable_->setShowGrid(true);

    auto* deviceStatusPanel = new QGroupBox(QStringLiteral("Device Status"), page);
    deviceStatusPanel->setObjectName(QStringLiteral("DeviceStatusPanel"));
    auto* deviceStatusLayout = new QVBoxLayout(deviceStatusPanel);
    deviceStatusLayout->setContentsMargins(8, 12, 8, 8);
    deviceStatusLayout->addWidget(diagnosticsTable_);
    deviceStatusLayout->addStretch();

    diagnosticsLatencyChart_ = new DiagnosticsLatencyChart(page);

    auto* chartPanel = new QGroupBox(QStringLiteral("Latency Analysis"), page);
    chartPanel->setObjectName(QStringLiteral("LatencyAnalysisPanel"));
    auto* chartLayout = new QVBoxLayout(chartPanel);
    chartLayout->setContentsMargins(8, 12, 8, 8);
    chartLayout->addWidget(diagnosticsLatencyChart_);

    auto* splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setObjectName(QStringLiteral("DiagnosticsSplitter"));
    splitter->addWidget(deviceStatusPanel);
    splitter->addWidget(chartPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(6);
    splitter->setSizes({kDiagTableWidth + 12, 680});
    layout->addWidget(splitter, 1);

    LeapConformanceMetrics empty{};
    populateDiagnosticsTable(empty);
}

void MainWindow::applySavedTheme() {
    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    const QString theme =
        settings.value(leap::studio::theme::kSettingsThemeKey, QStringLiteral("dark"))
            .toString();
    leap::studio::theme::ThemeManager::applyTheme(
        *static_cast<QApplication*>(QCoreApplication::instance()), theme);
}

void MainWindow::appendLog(const QString& line) {
    if (log_ == nullptr || line.isEmpty()) {
        return;
    }
    log_->appendPlainText(line);
}

void MainWindow::setStatusText(const QString& text) {
    statusBar()->showMessage(text, 0);
}

void MainWindow::showTab(int index) {
    if (mainTabs_ != nullptr && index >= 0 && index < mainTabs_->count()) {
        mainTabs_->setCurrentIndex(index);
    }
}

void MainWindow::setThemeDark() {
    leap::studio::theme::ThemeManager::applyTheme(
        *static_cast<QApplication*>(QCoreApplication::instance()),
        QStringLiteral("dark"));
    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    settings.setValue(leap::studio::theme::kSettingsThemeKey, QStringLiteral("dark"));
    if (diagnosticsLatencyChart_ != nullptr) {
        diagnosticsLatencyChart_->refreshTheme();
    }
}

void MainWindow::setThemeLight() {
    leap::studio::theme::ThemeManager::applyTheme(
        *static_cast<QApplication*>(QCoreApplication::instance()),
        QStringLiteral("light"));
    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    settings.setValue(leap::studio::theme::kSettingsThemeKey, QStringLiteral("light"));
    if (diagnosticsLatencyChart_ != nullptr) {
        diagnosticsLatencyChart_->refreshTheme();
    }
}

void MainWindow::runAutoBenchDemo(unsigned cyclicSeconds) {
    refreshAdapterList();
    if (diagnosticsLatencyChart_ != nullptr) {
        diagnosticsLatencyChart_->clearTrend();
    }
    resetDiagnosticsTrafficRates();
    showTab(4);
    setStatusText(QStringLiteral("Auto-bench: opening adapter…"));
    adapter_->openAdapter(selectedAdapterPath());
    QTimer::singleShot(
        800,
        this,
        [this, cyclicSeconds]() {
            setStatusText(QStringLiteral("Auto-bench: running conformance — watch Diagnostics"));
            adapter_->runScenario(
                selectedScenarioId(),
                {},
                selectedAdapterPath(),
                selectedAdapterLabel(),
                peerMacEdit_->text(),
                cyclicSeconds,
                100u);
        });
}

void MainWindow::onRunAll() {
    showTab(2);
    runProgress_->setValue(0);
    resultsTable_->setRowCount(0);
    if (diagnosticsLatencyChart_ != nullptr) {
        diagnosticsLatencyChart_->clearTrend();
    }
    resetDiagnosticsTrafficRates();
    unsigned cycPer = cyclePeriodSpin_ ? cyclePeriodSpin_->value() : 100u;
    adapter_->runScenario(selectedScenarioId(), {},
                          selectedAdapterPath(), selectedAdapterLabel(),
                          peerMacEdit_->text(), 2u, cycPer);
}

void MainWindow::onRunSelected() {
    QStringList steps;
    for (QCheckBox* cb : stepChecks_) {
        if (cb->isChecked()) {
            steps.append(cb->text());
        }
    }
    showTab(2);
    runProgress_->setValue(0);
    resultsTable_->setRowCount(0);
    unsigned cycPer = cyclePeriodSpin_ ? cyclePeriodSpin_->value() : 100u;
    adapter_->runScenario(selectedScenarioId(), steps,
                          selectedAdapterPath(), selectedAdapterLabel(),
                          peerMacEdit_->text(), 2u, cycPer);
}

void MainWindow::onStop() { adapter_->cancelRun(); }

void MainWindow::onProgressUpdated(const QString& stepName, unsigned percent) {
    if (runProgress_ != nullptr) {
        runProgress_->setValue(static_cast<int>(percent));
    }
    if (!stepName.isEmpty()) {
        setStatusText(QStringLiteral("Running: %1 (%2%)").arg(stepName).arg(percent));
    }
}

void MainWindow::onRunFinished(bool pass, const QString& summary) {
    if (runProgress_ != nullptr) {
        runProgress_->setValue(100);
    }
    if (runSummary_ != nullptr) {
        runSummary_->setText(
            QStringLiteral("%1 — %2")
                .arg(pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"), summary));
    }

    if (peerMacEdit_ != nullptr) {
        const QString key =
            leap::studio::discovery::normalizeMacKey(peerMacEdit_->text());
        if (!key.isEmpty()) {
            conformanceByMac_[key] = pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
            if (!lastDiscoveryPeers_.isEmpty()) {
                populateDiscoveryTable(lastDiscoveryPeers_);
            }
        }
    }

    setStatusText(summary);
    showTab(2);

    if (pass) {
        adapter_->refreshSnapshot();
    }
}

QString MainWindow::conformanceForMac(const QString& mac) const {
    const QString key = leap::studio::discovery::normalizeMacKey(mac);
    const auto it = conformanceByMac_.constFind(key);
    if (it != conformanceByMac_.constEnd()) {
        return it.value();
    }
    return QStringLiteral("—");
}

qint64 MainWindow::lastSeenForMac(const QString& mac) const {
    const QString key = leap::studio::discovery::normalizeMacKey(mac);
    const auto it = peerLastSeenMs_.constFind(key);
    if (it != peerLastSeenMs_.constEnd()) {
        return it.value();
    }
    return 0;
}

DiscoveryPeerRow MainWindow::deviceForExport() const {
    if (lastDiscoveryPeers_.isEmpty()) {
        return {};
    }

    const QString key =
        leap::studio::discovery::normalizeMacKey(peerMacEdit_->text());
    if (!key.isEmpty()) {
        for (const DiscoveryPeerRow& peer : lastDiscoveryPeers_) {
            if (leap::studio::discovery::normalizeMacKey(peer.mac) == key) {
                return peer;
            }
        }
    }

    return lastDiscoveryPeers_.first();
}

void MainWindow::populateDiscoveryTable(const QVector<DiscoveryPeerRow>& peers) {
    if (discoveryTable_ == nullptr) {
        return;
    }

    discoveryTable_->setRowCount(peers.size());
    for (int row = 0; row < peers.size(); ++row) {
        const DiscoveryPeerRow& peer = peers.at(row);
        const QString conformance = conformanceForMac(peer.mac);

        discoveryTable_->setItem(row, 0, new QTableWidgetItem(peer.mac));
        discoveryTable_->setItem(row, 1, new QTableWidgetItem(peer.platform));
        discoveryTable_->setItem(row, 2, new QTableWidgetItem(peer.product));
        discoveryTable_->setItem(row, 3, new QTableWidgetItem(peer.profile));

        const qint64 seenMs = lastSeenForMac(peer.mac);
        auto* stateItem = new QTableWidgetItem(
            leap::studio::discovery::stateWithLastSeen(peer.state, seenMs));
        const QColor bg = leap::studio::discovery::stateColor(peer.stateCode);
        stateItem->setBackground(QBrush(bg));
        if (peer.stateCode == LEAP_STATE_SAFE) {
            stateItem->setForeground(QBrush(QColor(0x1a, 0x1a, 0x1a)));
        } else {
            stateItem->setForeground(QBrush(Qt::white));
        }
        QFont stateFont = stateItem->font();
        stateFont.setBold(true);
        stateItem->setFont(stateFont);
        discoveryTable_->setItem(row, 4, stateItem);

        discoveryTable_->setItem(row, 5, new QTableWidgetItem(peer.leapVersion));
        discoveryTable_->setItem(row, 6, new QTableWidgetItem(peer.fw));
        discoveryTable_->setItem(row, 7, new QTableWidgetItem(peer.vendor));

        auto* confItem = new QTableWidgetItem(conformance);
        const QColor confColor =
            leap::studio::theme::conformanceStatusColor(conformance);
        if (confColor.isValid()) {
            confItem->setForeground(QBrush(confColor));
        }
        QFont confFont = confItem->font();
        confFont.setBold(conformance != QStringLiteral("—"));
        confItem->setFont(confFont);
        discoveryTable_->setItem(row, 8, confItem);
    }
    discoveryTable_->resizeColumnsToContents();
}

void MainWindow::refreshDiscoveryLastSeen() {
    if (discoveryTable_ == nullptr || lastDiscoveryPeers_.isEmpty()) {
        return;
    }

    for (int row = 0; row < lastDiscoveryPeers_.size(); ++row) {
        const DiscoveryPeerRow& peer = lastDiscoveryPeers_.at(row);
        QTableWidgetItem* stateItem = discoveryTable_->item(row, 4);
        if (stateItem == nullptr) {
            continue;
        }
        const qint64 seenMs = lastSeenForMac(peer.mac);
        stateItem->setText(
            leap::studio::discovery::stateWithLastSeen(peer.state, seenMs));
    }
}

void MainWindow::onDiscoveryPeers(const QVector<DiscoveryPeerRow>& peers) {
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const DiscoveryPeerRow& peer : peers) {
        peerLastSeenMs_[leap::studio::discovery::normalizeMacKey(peer.mac)] =
            nowMs;
    }

    lastDiscoveryPeers_ = peers;
    populateDiscoveryTable(peers);

    if (peers.size() == 1) {
        const DiscoveryPeerRow& peer = peers.first();
        if (peerMacEdit_ != nullptr) {
            peerMacEdit_->setText(peer.mac);
        }
    }

    setStatusText(QStringLiteral("Discovery: %1 peer(s)").arg(peers.size()));
}

void MainWindow::onConformanceRows(const QStringList& rows) {
    if (resultsTable_ == nullptr) {
        return;
    }
    resultsTable_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QStringList cols = rows.at(i).split(QLatin1Char('\t'));
        for (int c = 0; c < 4 && c < cols.size(); ++c) {
            auto* item = new QTableWidgetItem(cols.at(c));
            if (c == 2) {
                const QColor bg =
                    leap::studio::theme::conformanceStatusColor(cols.at(c));
                if (bg.isValid()) {
                    item->setBackground(QBrush(bg));
                    item->setForeground(QBrush(Qt::white));
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
            }
            resultsTable_->setItem(i, c, item);
        }
    }
    resultsTable_->resizeColumnsToContents();
}

void MainWindow::refreshAdapterList() {
    LeapRawWinpcapAdapterInfo adapters[LEAP_RAW_WINPCAP_ADAPTER_MAX];
    const size_t count = leap_raw_winpcap_enumerate_adapters(
        adapters,
        LEAP_RAW_WINPCAP_ADAPTER_MAX);
    QString previous = selectedAdapterPath();

    adapterCombo_->clear();
    for (size_t i = 0; i < count; ++i) {
        QString label = QString::fromUtf8(adapters[i].label);
        QString path = QString::fromUtf8(adapters[i].path);
        QString mac = QString::fromUtf8(adapters[i].mac);
        QString display = label.isEmpty() ? path : QStringLiteral("%1 — %2").arg(label, path);
        if (!mac.isEmpty()) {
            display += QStringLiteral("  %1").arg(mac);
        }
        adapterCombo_->addItem(display, path);
    }

    if (adapterCombo_->count() == 0) {
        adapterCombo_->addItem(
            QStringLiteral("(no adapters — install Npcap, run as Admin)"),
            QString());
        return;
    }

    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    const QString saved =
        settings.value(QStringLiteral("connection/lastAdapter")).toString();
    const QString pick = !previous.isEmpty() ? previous
                       : !saved.isEmpty()    ? saved
                                             : QString();

    int index = -1;
    if (!pick.isEmpty()) {
        index = adapterCombo_->findData(pick);
    }
    if (index < 0) {
        for (int i = 0; i < adapterCombo_->count(); ++i) {
            if (adapterCombo_->itemText(i).contains(
                    QStringLiteral("Ethernet 3"),
                    Qt::CaseInsensitive)) {
                index = i;
                break;
            }
        }
    }
    if (index >= 0) {
        adapterCombo_->setCurrentIndex(index);
    }
}

QString MainWindow::selectedAdapterPath() const {
    if (adapterCombo_ == nullptr) {
        return {};
    }
    return adapterCombo_->currentData().toString();
}

QString MainWindow::selectedAdapterLabel() const {
    if (adapterCombo_ == nullptr) {
        return {};
    }
    const QString text = adapterCombo_->currentText();
    const int sep = text.indexOf(QStringLiteral(" — "));
    return sep > 0 ? text.left(sep) : text;
}

QString MainWindow::selectedScenarioId() const {
    return QString();
}

void MainWindow::onListAdapters() {
    refreshAdapterList();
    appendLog(QStringLiteral("Adapters: %1 Npcap device(s)").arg(adapterCombo_->count()));
    showTab(0);
}

void MainWindow::onOpenAdapter() {
    const QString path = selectedAdapterPath();
    if (path.isEmpty()) {
        appendLog(QStringLiteral("[FAIL] select an adapter from the list"));
        return;
    }
    QSettings settings(leap::studio::theme::kOrgName, leap::studio::theme::kAppName);
    settings.setValue(QStringLiteral("connection/lastAdapter"), path);
    adapter_->openAdapter(path);
    if (connectionStatus_ != nullptr) {
        connectionStatus_->setText(QStringLiteral("Opening %1 …").arg(path));
    }
    showTab(0);
}

void MainWindow::onCloseAdapter() {
    adapter_->closeAdapter();
    if (connectionStatus_ != nullptr) {
        connectionStatus_->setText(QStringLiteral("Adapter closed."));
    }
}

void MainWindow::onDiscover() {
    showTab(1);
    adapter_->discover(selectedAdapterPath(), 3000);
}

void MainWindow::onIdentify() {
    showTab(1);
    adapter_->identifyPeer(selectedAdapterPath(), peerMacEdit_->text());
}

void MainWindow::onLocate() {
    showTab(1);
    adapter_->locatePeer(selectedAdapterPath(), peerMacEdit_->text(), 1500u);
}

void MainWindow::onStartMonitor() {
    adapter_->openAdapter(selectedAdapterPath());
    adapter_->startMonitor(250);
    showTab(3);
}

void MainWindow::onStopMonitor() { adapter_->stopMonitor(); }

void MainWindow::onRefreshSnapshot() {
    adapter_->openAdapter(selectedAdapterPath());
    adapter_->refreshSnapshot();
    showTab(3);
}

void MainWindow::resetDiagnosticsTrafficRates() {
    diagLastRxFrames_ = 0u;
    diagLastTxFrames_ = 0u;
    diagLastMetricsMs_ = 0;
}

void MainWindow::populateDiagnosticsTable(const LeapConformanceMetrics& metrics) {
    if (diagnosticsTable_ == nullptr) {
        return;
    }

    leap::studio::diagnostics::DiagnosticsTrafficRates rates;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (diagLastMetricsMs_ > 0) {
        const double deltaSec = static_cast<double>(nowMs - diagLastMetricsMs_) / 1000.0;
        if (deltaSec >= 0.05) {
            const qint64 rxDelta =
                static_cast<qint64>(metrics.rx_frames) -
                static_cast<qint64>(diagLastRxFrames_);
            const qint64 txDelta =
                static_cast<qint64>(metrics.tx_frames) -
                static_cast<qint64>(diagLastTxFrames_);
            if (rxDelta >= 0 && txDelta >= 0) {
                rates.rxFps = static_cast<double>(rxDelta) / deltaSec;
                rates.txFps = static_cast<double>(txDelta) / deltaSec;
                rates.hasRates = true;
            }
        }
    }
    diagLastRxFrames_ = metrics.rx_frames;
    diagLastTxFrames_ = metrics.tx_frames;
    diagLastMetricsMs_ = nowMs;

    QVector<leap::studio::diagnostics::DiagnosticsTableRow> rows;
    leap::studio::diagnostics::populateDiagnosticsRows(metrics, &rows, rates);

    diagnosticsTable_->clearSpans();
    diagnosticsTable_->setRowCount(rows.size());
    const QPalette palette = diagnosticsTable_->palette();
    const bool darkTheme =
        palette.color(QPalette::Window).lightness() < 128;
    const QColor sectionBackground =
        darkTheme ? QColor(0x3f, 0x3f, 0x3f) : QColor(0xe8, 0xe8, 0xe8);
    const QColor sectionForeground =
        darkTheme ? QColor(0xc8, 0xc8, 0xc8) : QColor(0x5a, 0x5a, 0x5a);
    const QFont baseFont = diagnosticsTable_->font();

    for (int row = 0; row < rows.size(); ++row) {
        const auto& entry = rows.at(row);
        diagnosticsTable_->setRowHeight(
            row,
            entry.kind == leap::studio::diagnostics::DiagnosticsTableRow::Kind::Section
                ? 22
                : 24);

        auto* fieldItem = diagnosticsTable_->item(row, 0);
        if (fieldItem == nullptr) {
            fieldItem = new QTableWidgetItem();
            diagnosticsTable_->setItem(row, 0, fieldItem);
        }

        auto* valueItem = diagnosticsTable_->item(row, 1);
        if (valueItem == nullptr) {
            valueItem = new QTableWidgetItem();
            diagnosticsTable_->setItem(row, 1, valueItem);
        }

        if (entry.kind ==
            leap::studio::diagnostics::DiagnosticsTableRow::Kind::Section) {
            diagnosticsTable_->setSpan(row, 0, 1, 2);
            fieldItem->setText(entry.label);
            fieldItem->setBackground(QBrush(sectionBackground));
            fieldItem->setForeground(QBrush(sectionForeground));
            QFont sectionFont = fieldItem->font();
            sectionFont.setBold(true);
            sectionFont.setPointSizeF(sectionFont.pointSizeF() - 0.5);
            fieldItem->setFont(sectionFont);
            fieldItem->setFlags(Qt::ItemIsEnabled);
            valueItem->setText(QString());
            valueItem->setBackground(QBrush(sectionBackground));
            valueItem->setFlags(Qt::ItemIsEnabled);
            continue;
        }

        diagnosticsTable_->setSpan(row, 0, 1, 1);
        fieldItem->setText(entry.label);
        fieldItem->setBackground(QBrush());
        fieldItem->setForeground(QBrush());
        fieldItem->setFont(baseFont);
        fieldItem->setFlags((fieldItem->flags() | Qt::ItemIsSelectable) &
                            ~Qt::ItemIsEditable);

        valueItem->setText(entry.value);
        valueItem->setToolTip(entry.value);
        valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        valueItem->setBackground(QBrush());
        valueItem->setForeground(QBrush());
        valueItem->setFont(baseFont);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);

        if (entry.label == QStringLiteral("Device State")) {
            const uint16_t stateCode =
                diagnosticsStateCode(metrics, entry.value);
            if (stateCode != 0u) {
                const QColor bg =
                    leap::studio::discovery::stateColor(stateCode);
                valueItem->setBackground(QBrush(bg));
                if (stateCode == LEAP_STATE_SAFE) {
                    valueItem->setForeground(QBrush(QColor(0x1a, 0x1a, 0x1a)));
                } else {
                    valueItem->setForeground(QBrush(Qt::white));
                }
                QFont stateFont = valueItem->font();
                stateFont.setBold(true);
                valueItem->setFont(stateFont);
            }
        }
    }

    if (diagnosticsLatencyChart_ != nullptr) {
        diagnosticsLatencyChart_->applyMetrics(metrics);
    }
}

void MainWindow::onStartDiagnostics() {
    resetDiagnosticsTrafficRates();
    adapter_->openAdapter(selectedAdapterPath());
    adapter_->startMonitor(500);
    showTab(4);
}

void MainWindow::onStopDiagnostics() { adapter_->stopMonitor(); }

void MainWindow::onRefreshDiagnostics() {
    adapter_->openAdapter(selectedAdapterPath());
    adapter_->refreshSnapshot();
    showTab(4);
}

void MainWindow::onExportReport() {
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultName =
        QStringLiteral("leap_bench_%1.md").arg(stamp);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export conformance report"),
        defaultName,
        QStringLiteral("Markdown (*.md);;CSV (*.csv);;JSON (*.json)"));
    if (!path.isEmpty()) {
        const DiscoveryPeerRow device = deviceForExport();
        adapter_->exportReport(path, device, !device.mac.isEmpty());
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveMainSplitter();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveMainSplitter() {
    if (mainVerticalSplitter_ == nullptr) {
        return;
    }
    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    settings.setValue(QStringLiteral("ui/mainSplitter"),
                      mainVerticalSplitter_->saveState());
}

void MainWindow::restoreMainSplitter() {
    if (mainVerticalSplitter_ == nullptr) {
        return;
    }

    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    const QByteArray state =
        settings.value(QStringLiteral("ui/mainSplitter")).toByteArray();
    if (!state.isEmpty()) {
        mainVerticalSplitter_->restoreState(state);
        return;
    }

    const int total = mainVerticalSplitter_->height();
    if (total > 0) {
        mainVerticalSplitter_->setSizes({total * 7 / 10, total * 3 / 10});
    }
}

uint16_t MainWindow::diagnosticsStateCode(const LeapConformanceMetrics& metrics,
                                          const QString& stateText) const {
    if (metrics.device_state != 0u) {
        return metrics.device_state;
    }

    if (metrics.stack_phase == LEAP_CTRL_STACK_OP) {
        return LEAP_STATE_OP;
    }

    if (stateText == QStringLiteral("SAFE")) {
        return LEAP_STATE_SAFE;
    }
    if (stateText == QStringLiteral("OP")) {
        return LEAP_STATE_OP;
    }
    if (stateText == QStringLiteral("FAULT")) {
        return LEAP_STATE_FAULT;
    }
    if (stateText == QStringLiteral("INIT")) {
        return LEAP_STATE_INIT;
    }
    if (stateText == QStringLiteral("BOOT")) {
        return LEAP_STATE_BOOT;
    }
    if (stateText == QStringLiteral("CONFIGURED")) {
        return LEAP_STATE_CONFIGURED;
    }

    return 0u;
}
