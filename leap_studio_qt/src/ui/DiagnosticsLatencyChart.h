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
    explicit DiagnosticsLatencyChart(QWidget* parent = nullptr);

    void applyMetrics(const LeapConformanceMetrics& metrics);
    void clearTrend();
    void refreshTheme();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct LatencyStats {
        int count = 0;
        int lastUs = 0;
        int minUs = 0;
        int maxUs = 0;
        double avgUs = 0.0;
        double stdUs = 0.0;
    };

    struct HealthSummary {
        QString label;
        QColor labelColor;
        uint64_t staleFrames = 0u;
        uint64_t duplicateFrames = 0u;
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
                        const QVector<double>& rollingAvg);
    void paintHistogram(QPainter& painter, const QRect& area,
                        const QVector<int>& bins, int maxCount);

    qreal mapY(int latencyUs, qreal yMax, const QRect& plot) const;

    QVector<int> samples_;
    uint32_t baseExchange_ = 0u;
    uint64_t staleFrames_ = 0u;
    uint64_t duplicateFrames_ = 0u;

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
