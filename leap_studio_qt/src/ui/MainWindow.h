#pragma once

#include "core/DiscoveryPeer.h"

extern "C" {
#include "leap/conformance/leap_conformance_metrics.h"
}

#include <QHash>
#include <QMainWindow>
#include <QVector>

class ConformanceAdapter;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QTimer;
class QTreeWidget;
class DiagnosticsLatencyChart;

class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void runAutoBenchDemo(unsigned cyclicSeconds = 10u);

protected:
    void closeEvent(QCloseEvent* event) override;

public slots:
    void appendLog(const QString& line);
    bool shouldAcceptRunLog(const QString& line);

private slots:
    void setThemeDark();
    void setThemeLight();
    void onRunAll();
    void onRunSelected();
    void onRunIoBench();
    void onPrepareIoSession();
    void onIoSessionReady(bool ok, const QString& detail);
    void onStop();
    void onDiscover();
    void onIdentify();
    void onLocate();
    void onStartMonitor();
    void onStopMonitor();
    void onRefreshSnapshot();
    void onStartDiagnostics();
    void onStopDiagnostics();
    void onRefreshDiagnostics();
    void onExportReport();
    void onListAdapters();
    void onOpenAdapter();
    void onCloseAdapter();
    void onProgressUpdated(const QString& stepName, unsigned percent);
    void onRunFinished(bool pass, const QString& summary, quint64 runToken);
    void onConformanceRows(const QStringList& rows, quint64 runToken);
    void onDiscoveryPeers(const QVector<DiscoveryPeerRow>& peers);
    void refreshDiscoveryLastSeen();

private:
    void shutdownForExit();
    void buildUi();
    void buildConnectionTab(QWidget* page);
    void buildDiscoveryTab(QWidget* page);
    void buildConformanceTab(QWidget* page);
    void buildIoPerformanceTab(QWidget* page);
    void buildMonitorTab(QWidget* page);
    void buildDiagnosticsTab(QWidget* page);
    void applySavedTheme();
    void refreshAdapterList();
    QString selectedAdapterPath() const;
    QString selectedAdapterLabel() const;
    QString selectedScenarioId() const;
    void setStatusText(const QString& text);
    void showTab(int index);
    int tabIndexByLabel(const QString& label) const;
    void populateDiscoveryTable(const QVector<DiscoveryPeerRow>& peers);
    void populateDiagnosticsTable(const LeapConformanceMetrics& metrics);
    void populateIoPerformanceStats(const LeapConformanceMetrics& metrics);
    void updateMonitorMetricsTable(const LeapConformanceMetrics& metrics);
    void updateIoBenchMetrics(const LeapConformanceMetrics& metrics);
    void setIoBenchRunActive(bool active);
    bool isIoBenchStepName(const QString& stepName) const;
    void resetDiagnosticsTrafficRates();
    void restoreMainSplitter();
    void saveMainSplitter();
    uint16_t diagnosticsStateCode(const LeapConformanceMetrics& metrics,
                                  const QString& stateText) const;
    QString conformanceForMac(const QString& mac) const;
    DiscoveryPeerRow deviceForExport() const;
    qint64 lastSeenForMac(const QString& mac) const;

    static void leapLogSink(void* ctx, int is_error, const char* line);

    ConformanceAdapter* adapter_ = nullptr;
    QTabWidget* mainTabs_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QComboBox* adapterCombo_ = nullptr;
    QLineEdit* peerMacEdit_ = nullptr;
    QSpinBox*  cyclePeriodSpin_ = nullptr;
    QLabel* connectionStatus_ = nullptr;
    QTableWidget* discoveryTable_ = nullptr;
    QHash<QString, QString> conformanceByMac_;
    QHash<QString, qint64> peerLastSeenMs_;
    QVector<DiscoveryPeerRow> lastDiscoveryPeers_;
    QTimer* lastSeenTimer_ = nullptr;
    QTableWidget* resultsTable_ = nullptr;
    QProgressBar* runProgress_ = nullptr;
    QLabel* runSummary_ = nullptr;
    QTableWidget* metricsTable_ = nullptr;
    QTableWidget* diagnosticsTable_ = nullptr;
    DiagnosticsLatencyChart* diagnosticsLatencyChart_ = nullptr;
    DiagnosticsLatencyChart* ioPerformanceChart_ = nullptr;
    QSpinBox* ioBenchSoakSecondsSpin_ = nullptr;
    QSpinBox* ioBenchCycleMsSpin_ = nullptr;
    QCheckBox* ioBenchDiagPollCheck_ = nullptr;
    QProgressBar* ioBenchProgress_ = nullptr;
    QLabel* ioBenchSummary_ = nullptr;
    QLabel* ioBenchSloLabel_ = nullptr;
    QLabel* ioSessionStatus_ = nullptr;
    QPushButton* ioSessionButton_ = nullptr;
    QTableWidget* ioBenchStatsTable_ = nullptr;
    uint64_t ioLastExchangeReplies_ = 0u;
    qint64 ioLastExchangeMs_ = 0;
    bool monitorLiveActive_ = false;
    bool diagnosticsLiveActive_ = false;
    bool diagnosticsSnapshotPending_ = false;
    bool ioSessionConnected_ = false;
    bool ioBenchRunActive_ = false;
    quint64 ioBenchRunToken_ = 0;
    quint64 conformanceRunToken_ = 0;
    bool shuttingDown_ = false;
    bool suppressRunLogs_ = false;
    bool stopPending_ = false;
    qint64 logBurstWindowMs_ = 0;
    int logBurstCount_ = 0;
    uint64_t diagLastRxFrames_ = 0u;
    uint64_t diagLastTxFrames_ = 0u;
    qint64 diagLastMetricsMs_ = 0;
    QSplitter* mainVerticalSplitter_ = nullptr;
    QList<QCheckBox*> stepChecks_;
};
