#pragma once

#include "core/DiscoveryPeer.h"

extern "C" {
#include "leap/conformance/leap_conformance_result.h"
}

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVector>

struct LeapConformanceMetrics;

class ConformanceWorker;

class ConformanceAdapter : public QObject {
    Q_OBJECT
public:
    explicit ConformanceAdapter(QObject* parent = nullptr);
    ~ConformanceAdapter() override;

public slots:
    void openAdapter(const QString& adapterPath, const QString& capturePcap = {});
    void closeAdapter();
    void runScenario(const QString& scenarioId, const QStringList& stepFilter,
                     const QString& adapterPath, const QString& adapterLabel,
                     const QString& peerMac, unsigned cyclicSeconds,
                     unsigned cyclicPeriodMs = 100u);
    void exportReport(const QString& path, const DiscoveryPeerRow& device,
                      bool hasDevice);
    void cancelRun();
    void discover(const QString& adapterPath, int scanMs);
    void identifyPeer(const QString& adapterPath, const QString& peerMac);
    void locatePeer(const QString& adapterPath, const QString& peerMac,
                     unsigned durationMs);
    void startMonitor(unsigned intervalMs);
    void stopMonitor();
    void refreshSnapshot();

signals:
    void logLine(const QString& line);
    void runFinished(bool pass, const QString& summary);
    void progressUpdated(const QString& stepName, unsigned percent);
    void metricsUpdated(const LeapConformanceMetrics& metrics);
    void discoveryPeers(const QVector<DiscoveryPeerRow>& peers);
    void conformanceRows(const QStringList& rows);
    void exportFinished(bool ok, const QString& path, const QString& detail);

private:
    void ensureWorkerThread();
    void appendLog(const char* fmt, ...);

    QThread workerThread_;
    ConformanceWorker* worker_ = nullptr;
    bool monitorActive_ = false;
    QString adapterPath_;
    bool hasLastResult_ = false;
    LeapConformanceRunResult lastResult_{};
    QString lastRunStartedLocal_;
    QString lastNicName_;
    unsigned lastCyclicSeconds_ = 0u;
    unsigned lastCyclicPeriodMs_ = 100u;
};
