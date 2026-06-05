#pragma once

#include <QObject>

class QTimer;

struct LeapConformanceIo;
struct LeapConformanceWinContext;

class ConformanceWorker : public QObject {
    Q_OBJECT
public:
    explicit ConformanceWorker(QObject* parent = nullptr);
    ~ConformanceWorker() override;

    LeapConformanceIo* io();
    LeapConformanceWinContext* ctx() { return ctx_; }

    void setPeerMac(const uint8_t* mac, int hasMac);
    QTimer* monitorTimer() const { return monitorTimer_; }
    void setMonitorTimer(QTimer* timer) { monitorTimer_ = timer; }

private:
    LeapConformanceWinContext* ctx_ = nullptr;
    const LeapConformanceIo* io_ = nullptr;
    QTimer* monitorTimer_ = nullptr;
};
