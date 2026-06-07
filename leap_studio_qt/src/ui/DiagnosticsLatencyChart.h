#pragma once

extern "C" {
#include "leap/conformance/leap_conformance_metrics.h"
}

#include <QColor>
#include <QVector>
#include <QWidget>

class DiagnosticsLatencyChart : public QWidget {
    Q_OBJECT
public:
    enum class ChartMode {
        ReplyLatency,
        WireRtt,
    };

    explicit DiagnosticsLatencyChart(QWidget* parent = nullptr);

    void setChartMode(ChartMode mode);
    ChartMode chartMode() const { return chartMode_; }

    void applyMetrics(const LeapConformanceMetrics& metrics);
    void clearTrend();
    void refreshTheme();
    void setLiveUpdatesEnabled(bool enabled);
    bool liveUpdatesEnabled() const { return liveUpdatesEnabled_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct LatencyStats {
        int count = 0;
        int lastUs = 0;
        int minUs = 0;
        int maxUs = 0;
        int p99Us = 0;
        int p999Us = 0;
        double avgUs = 0.0;
        double stdUs = 0.0;
    };

    struct HealthSummary {
        QString label;
        QColor labelColor;
        uint64_t staleFrames = 0u;
        uint64_t duplicateFrames = 0u;
        uint64_t recvTimeouts = 0u;
        uint64_t exchangeReplies = 0u;
        uint64_t cyclesCompleted = 0u;
        int worstLatencyUs = 0;
    };

    LatencyStats computeStats() const;
    HealthSummary computeHealth(const LatencyStats& stats) const;
    QVector<double> computeRollingAverage() const;
    QVector<int> computeHistogram(int* maxCountOut) const;
    QVector<int> sparklineSamples(int maxPoints) const;

    void paintHealthCard(QPainter& painter, const QRect& area,
                         const HealthSummary& health);
    void paintStatCards(QPainter& painter, const QRect& area, const LatencyStats& stats);
    void paintSparkline(QPainter& painter, const QRect& area,
                        const QVector<int>& samples, const QColor& color) const;
    void paintLineChart(QPainter& painter, const QRect& plot,
                        const QVector<double>& rollingAvg, int yMaxUs);
    void paintHistogram(QPainter& painter, const QRect& area,
                        const QVector<int>& bins, int maxCount,
                        const QVector<int>& binEdges, int axisMaxUs);

    qreal mapY(int latencyUs, qreal yMax, const QRect& plot) const;

    QVector<int> samples_;
    QString trendTitle_ = QStringLiteral("Reply Latency Trend");
    uint32_t baseExchange_ = 0u;
    uint64_t staleFrames_ = 0u;
    uint64_t duplicateFrames_ = 0u;
    uint64_t recvTimeouts_ = 0u;
    uint64_t exchangeReplies_ = 0u;
    uint64_t cyclesCompleted_ = 0u;
    uint64_t replyRejects_ = 0u;
    bool hasPdRttAggregate_ = false;
    int pdRttLastUs_ = 0;
    int pdRttAvgUs_ = 0;
    int pdRttMinUs_ = 0;
    int pdRttMaxUs_ = 0;
    ChartMode chartMode_ = ChartMode::ReplyLatency;
    bool liveUpdatesEnabled_ = true;

    QColor panelBackground_;
    QColor plotBackground_;
    QColor textColor_;
    QColor mutedTextColor_;
    QColor rawLineColor_;
    QColor avgLineColor_;
    QColor gridColor_;
    QColor zoneGreen_;
    QColor zoneYellow_;
    QColor zoneRed_;
    QColor thresholdColor_;
    QColor histogramGreenBar_;
    QColor histogramAmberBar_;
    QColor histogramRedBar_;
    QColor cardBorderColor_;
    QColor cardFillColor_;
    QColor cardValueColor_;
};
