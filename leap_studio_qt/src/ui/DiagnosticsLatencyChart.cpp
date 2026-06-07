#include "ui/DiagnosticsLatencyChart.h"

#include "leap/conformance/leap_conformance_scenario.h"
#include "ui/theme/StatusPalette.h"

#include <climits>

#include <QApplication>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QtMath>
#include <algorithm>

namespace {

constexpr int kMarginLeft = 48;
constexpr int kMarginRight = 40;
constexpr int kHealthCardWidth = 236;
constexpr int kHealthCardHeight = 92;
constexpr int kStatsCardsHeight = 88;
constexpr int kSectionGap = 8;
constexpr int kRollingWindow = 30;
constexpr int kSparklinePoints = 100;
constexpr int kLineYMaxUs = 1000;
constexpr int kWarnLatencyUs = 750;
constexpr int kStatGreenMaxUs = 250;
constexpr int kStatYellowMaxUs = 500;

QColor latencyStatColor(int micros, DiagnosticsLatencyChart::ChartMode mode) {
    if (mode == DiagnosticsLatencyChart::ChartMode::WireRtt) {
        if (micros <= static_cast<int>(LEAP_CONF_IO_BENCH_MAX_RTT_US)) {
            return leap::studio::theme::statusPass();
        }
        if (micros <= static_cast<int>(LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US)) {
            return leap::studio::theme::statusWarn();
        }
        return leap::studio::theme::statusFail();
    }

    if (micros < kStatGreenMaxUs) {
        return leap::studio::theme::statusPass();
    }
    if (micros <= kStatYellowMaxUs) {
        return leap::studio::theme::statusWarn();
    }
    return leap::studio::theme::statusFail();
}

constexpr int kHistBinEdges[] = {0, 100, 200, 300, 400, 500, 1000};
constexpr int kHistBinCount = 6;

int percentileFromSamples(const QVector<int>& samples, unsigned permille) {
    if (samples.isEmpty() || permille == 0u || permille > 1000u) {
        return 0;
    }

    QVector<int> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    const uint64_t rank =
        (static_cast<uint64_t>(sorted.size()) * static_cast<uint64_t>(permille) +
         999u) /
        1000u;
    const int index = static_cast<int>(rank == 0u ? 0u : rank - 1u);
    return sorted.at(qBound(0, index, sorted.size() - 1));
}

constexpr int kThresholdsUs[] = {250, 750, 1000};
constexpr int kHistogramAxisLabelsUs[] = {0, 100, 200, 300, 400, 500, 1000};

int niceWireRttChartYMaxUs(int dataMaxUs) {
    if (dataMaxUs <= kLineYMaxUs) {
        return kLineYMaxUs;
    }

    const int padded = (dataMaxUs * 12) / 10;
    const int rounded = ((padded + 4999) / 5000) * 5000;
    return qMax(10000, rounded);
}

QVector<int> wireRttHistogramBinEdges(int axisMaxUs) {
    constexpr int kWireHistBinCount = 6;
    QVector<int> edges;
    edges.reserve(kWireHistBinCount + 1);
    for (int i = 0; i <= kWireHistBinCount; ++i) {
        edges.append((axisMaxUs * i) / kWireHistBinCount);
    }
    return edges;
}

QVector<int> wireRttHistogramAxisLabels(int axisMaxUs) {
    QVector<int> labels;
    labels << 0 << (axisMaxUs / 4) << (axisMaxUs / 2)
           << ((axisMaxUs * 3) / 4) << axisMaxUs;
    return labels;
}

QVector<int> downsampleSparkline(const QVector<int>& series, int maxPoints) {
    if (series.isEmpty() || maxPoints <= 0) {
        return {};
    }
    if (series.size() <= maxPoints) {
        return series;
    }

    QVector<int> out;
    out.reserve(maxPoints);
    for (int i = 0; i < maxPoints; ++i) {
        const int index = (i * (series.size() - 1)) / (maxPoints - 1);
        out.append(series.at(index));
    }
    return out;
}

QVector<int> rollingAvgSparkline(const QVector<int>& samples, int window,
                                 int maxPoints) {
    if (samples.isEmpty()) {
        return {};
    }

    QVector<int> rolling(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        const int start = qMax(0, i - window + 1);
        double sum = 0.0;
        for (int j = start; j <= i; ++j) {
            sum += samples.at(j);
        }
        rolling[i] = static_cast<int>(qRound(sum / static_cast<double>(i - start + 1)));
    }
    return downsampleSparkline(rolling, maxPoints);
}

QVector<int> rollingMinSparkline(const QVector<int>& samples, int window,
                                 int maxPoints) {
    if (samples.isEmpty()) {
        return {};
    }

    QVector<int> rolling(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        const int start = qMax(0, i - window + 1);
        int minV = samples.at(start);
        for (int j = start + 1; j <= i; ++j) {
            minV = qMin(minV, samples.at(j));
        }
        rolling[i] = minV;
    }
    return downsampleSparkline(rolling, maxPoints);
}

QVector<int> rollingStdSparkline(const QVector<int>& samples, int window,
                                 int maxPoints) {
    if (samples.isEmpty()) {
        return {};
    }

    QVector<int> rolling(samples.size());
    for (int i = 0; i < samples.size(); ++i) {
        const int start = qMax(0, i - window + 1);
        const int count = i - start + 1;
        double sum = 0.0;
        for (int j = start; j <= i; ++j) {
            sum += samples.at(j);
        }
        const double avg = sum / static_cast<double>(count);
        double variance = 0.0;
        for (int j = start; j <= i; ++j) {
            const double delta = samples.at(j) - avg;
            variance += delta * delta;
        }
        rolling[i] = static_cast<int>(qRound(qSqrt(variance / count)));
    }
    return downsampleSparkline(rolling, maxPoints);
}

}  // namespace

DiagnosticsLatencyChart::DiagnosticsLatencyChart(QWidget* parent)
    : QWidget(parent) {
    setMinimumWidth(300);
    setMinimumHeight(460);
    refreshTheme();
}

void DiagnosticsLatencyChart::refreshTheme() {
    const QPalette palette = QApplication::palette();
    const QColor window = palette.color(QPalette::Window);
    const bool dark = window.lightness() < 128;

    panelBackground_ = window;
    plotBackground_ = QColor(QStringLiteral("#ffffff"));
    textColor_ = palette.color(QPalette::WindowText);
    mutedTextColor_ = dark ? QColor(QStringLiteral("#b0b0b0"))
                           : QColor(QStringLiteral("#606060"));
    rawLineColor_ = QColor(QStringLiteral("#4a90d9"));
    avgLineColor_ = QColor(QStringLiteral("#e67e22"));
    gridColor_ = QColor(QStringLiteral("#f0f0f0"));
    zoneGreen_ = QColor(220, 245, 228, 90);
    zoneYellow_ = QColor(255, 248, 210, 90);
    zoneRed_ = QColor(255, 228, 228, 90);
    thresholdColor_ = QColor(QStringLiteral("#dddddd"));
    histogramGreenBar_ = QColor(0xa8, 0xcb, 0xb8);
    histogramAmberBar_ = QColor(0xd4, 0xc4, 0x98);
    histogramRedBar_ = QColor(0xd4, 0xa8, 0xa8);
    cardBorderColor_ = dark ? QColor(QStringLiteral("#555555"))
                            : QColor(QStringLiteral("#d4d4d4"));
    cardFillColor_ = dark ? QColor(QStringLiteral("#3a3a3a"))
                          : QColor(QStringLiteral("#fafafa"));
    cardValueColor_ = dark ? QColor(QStringLiteral("#f2f2f2"))
                           : QColor(QStringLiteral("#1a1a1a"));

    update();
}

void DiagnosticsLatencyChart::setChartMode(ChartMode mode) {
    if (chartMode_ == mode) {
        return;
    }
    chartMode_ = mode;
    clearTrend();
    update();
}

void DiagnosticsLatencyChart::setLiveUpdatesEnabled(bool enabled) {
    liveUpdatesEnabled_ = enabled;
}

void DiagnosticsLatencyChart::clearTrend() {
    samples_.clear();
    trendTitle_ = chartMode_ == ChartMode::WireRtt
                      ? QStringLiteral("Wire RTT (soak only)")
                      : QStringLiteral("Reply Latency Trend");
    baseExchange_ = 0u;
    staleFrames_ = 0u;
    duplicateFrames_ = 0u;
    recvTimeouts_ = 0u;
    exchangeReplies_ = 0u;
    cyclesCompleted_ = 0u;
    replyRejects_ = 0u;
    hasPdRttAggregate_ = false;
    pdRttLastUs_ = 0;
    pdRttAvgUs_ = 0;
    pdRttMinUs_ = 0;
    pdRttMaxUs_ = 0;
    update();
}

void DiagnosticsLatencyChart::applyMetrics(const LeapConformanceMetrics& metrics) {
    if (!liveUpdatesEnabled_) {
        return;
    }

    staleFrames_ = metrics.stale_frames;
    duplicateFrames_ = metrics.duplicate_frames;
    recvTimeouts_ = metrics.pd.recv_timeouts;
    exchangeReplies_ = metrics.pd.exchange_replies;
    cyclesCompleted_ = metrics.pd.cycles_completed;
    replyRejects_ = metrics.pd.reply_rejects;
    hasPdRttAggregate_ = metrics.pd.network_rtt_samples > 0u;
    if (hasPdRttAggregate_) {
        pdRttLastUs_ = static_cast<int>(
            qMin<uint64_t>(metrics.pd.last_network_rtt_us, INT_MAX));
        pdRttMaxUs_ = static_cast<int>(
            qMin<uint64_t>(metrics.pd.max_network_rtt_us, INT_MAX));
        pdRttAvgUs_ = static_cast<int>(qMin<uint64_t>(
            metrics.pd.total_network_rtt_us / metrics.pd.network_rtt_samples,
            INT_MAX));
        pdRttMinUs_ = pdRttLastUs_;
        if (metrics.network_rtt_trend.count > 0u) {
            uint32_t minTrend = UINT32_MAX;
            for (uint32_t i = 0u; i < metrics.network_rtt_trend.count; ++i) {
                minTrend = qMin(minTrend, metrics.network_rtt_trend.samples[i]);
            }
            if (minTrend != UINT32_MAX) {
                pdRttMinUs_ = static_cast<int>(minTrend);
            }
        }
    } else {
        pdRttLastUs_ = 0;
        pdRttAvgUs_ = 0;
        pdRttMinUs_ = 0;
        pdRttMaxUs_ = 0;
    }

    const bool useWireRtt =
        chartMode_ == ChartMode::WireRtt ||
        (metrics.network_rtt_trend.count > 0u &&
         metrics.network_rtt_trend.count >= metrics.reply_latency_trend.count);
    const LeapConformanceLatencyTrend& trend =
        useWireRtt ? metrics.network_rtt_trend : metrics.reply_latency_trend;

    trendTitle_ = useWireRtt ? QStringLiteral("Wire RTT (soak only)")
                             : QStringLiteral("Reply Latency Trend");

    samples_.resize(static_cast<int>(trend.count));
    for (uint32_t i = 0u; i < trend.count; i++) {
        samples_[static_cast<int>(i)] = static_cast<int>(trend.samples[i]);
    }
    baseExchange_ = trend.count > 0u ? trend.base_exchange : 0u;
    update();
}

DiagnosticsLatencyChart::LatencyStats DiagnosticsLatencyChart::computeStats() const {
    LatencyStats stats;
    if (samples_.isEmpty()) {
        if (chartMode_ == ChartMode::WireRtt && hasPdRttAggregate_) {
            stats.count = 1;
            stats.lastUs = pdRttLastUs_;
            stats.minUs = pdRttMinUs_;
            stats.maxUs = pdRttMaxUs_;
            stats.avgUs = pdRttAvgUs_;
            return stats;
        }
        return stats;
    }

    stats.count = samples_.size();
    stats.lastUs = samples_.constLast();
    stats.minUs = samples_.constFirst();
    stats.maxUs = samples_.constFirst();
    double sum = 0.0;

    for (int sample : samples_) {
        stats.minUs = qMin(stats.minUs, sample);
        stats.maxUs = qMax(stats.maxUs, sample);
        sum += sample;
    }

    stats.avgUs = sum / stats.count;
    double variance = 0.0;
    for (int sample : samples_) {
        const double delta = sample - stats.avgUs;
        variance += delta * delta;
    }
    stats.stdUs = qSqrt(variance / stats.count);
    stats.p99Us = percentileFromSamples(samples_, 990u);
    stats.p999Us = percentileFromSamples(samples_, 999u);
    return stats;
}

DiagnosticsLatencyChart::HealthSummary DiagnosticsLatencyChart::computeHealth(
    const LatencyStats& stats) const {
    HealthSummary health;
    health.staleFrames = staleFrames_;
    health.duplicateFrames = duplicateFrames_;
    health.recvTimeouts = recvTimeouts_;
    health.exchangeReplies = exchangeReplies_;
    health.cyclesCompleted = cyclesCompleted_;
    health.worstLatencyUs = stats.maxUs;

    const bool soakHealth = chartMode_ == ChartMode::WireRtt;
    const bool hasSoakFailures =
        soakHealth &&
        (recvTimeouts_ > 0u ||
         (cyclesCompleted_ > 0u && exchangeReplies_ < cyclesCompleted_));
    const bool latencyWarn =
        stats.count > 0 &&
        (soakHealth ? (stats.p999Us > LEAP_CONF_IO_BENCH_MAX_RTT_US ||
                       stats.maxUs > LEAP_CONF_IO_BENCH_MAX_RTT_US)
                    : (stats.maxUs >= kWarnLatencyUs));
    const bool latencySevere =
        stats.count > 0 &&
        (soakHealth ? (stats.p99Us > LEAP_CONF_IO_BENCH_MAX_RTT_US ||
                       stats.maxUs > LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US)
                    : (stats.maxUs >= 1000));
    const bool hasIssues =
        hasSoakFailures || staleFrames_ > 0u || duplicateFrames_ > 0u ||
        latencyWarn;
    const bool severe =
        hasSoakFailures || staleFrames_ > 0u || duplicateFrames_ > 0u ||
        latencySevere;

    if (stats.count == 0 && !soakHealth) {
        health.label = QStringLiteral("NO DATA");
        health.labelColor = leap::studio::theme::statusNeutral();
    } else if (stats.count == 0 && soakHealth && cyclesCompleted_ == 0u) {
        health.label = QStringLiteral("NO DATA");
        health.labelColor = leap::studio::theme::statusNeutral();
    } else if (severe) {
        health.label = QStringLiteral("ATTENTION");
        health.labelColor = leap::studio::theme::statusFail();
    } else if (hasIssues) {
        health.label = QStringLiteral("WARN");
        health.labelColor = leap::studio::theme::statusWarn();
    } else {
        health.label = QStringLiteral("GOOD");
        health.labelColor = leap::studio::theme::statusPass();
    }

    return health;
}

QVector<double> DiagnosticsLatencyChart::computeRollingAverage() const {
    QVector<double> rolling;
    rolling.resize(samples_.size());
    if (samples_.isEmpty()) {
        return rolling;
    }

    for (int i = 0; i < samples_.size(); ++i) {
        const int start = qMax(0, i - kRollingWindow + 1);
        double sum = 0.0;
        for (int j = start; j <= i; ++j) {
            sum += samples_.at(j);
        }
        rolling[i] = sum / static_cast<double>(i - start + 1);
    }
    return rolling;
}

QVector<int> DiagnosticsLatencyChart::computeHistogram(int* maxCountOut) const {
    QVector<int> bins;
    int maxCount = 0;

    if (samples_.isEmpty()) {
        if (maxCountOut != nullptr) {
            *maxCountOut = 0;
        }
        return bins;
    }

    if (chartMode_ == ChartMode::WireRtt) {
        int dataMax = 0;
        for (int sample : samples_) {
            dataMax = qMax(dataMax, sample);
        }
        const int axisMax = niceWireRttChartYMaxUs(dataMax);
        const QVector<int> edges = wireRttHistogramBinEdges(axisMax);
        const int binCount = edges.size() - 1;
        bins = QVector<int>(binCount, 0);

        for (int sample : samples_) {
            int index = binCount - 1;
            for (int i = 0; i < binCount; ++i) {
                if (sample < edges.at(i + 1)) {
                    index = i;
                    break;
                }
            }
            bins[index]++;
            maxCount = qMax(maxCount, bins[index]);
        }
    } else {
        bins = QVector<int>(kHistBinCount, 0);
        for (int sample : samples_) {
            int index = kHistBinCount - 1;
            for (int i = 0; i < kHistBinCount; ++i) {
                if (sample < kHistBinEdges[i + 1]) {
                    index = i;
                    break;
                }
            }
            bins[index]++;
            maxCount = qMax(maxCount, bins[index]);
        }
    }

    if (maxCountOut != nullptr) {
        *maxCountOut = maxCount;
    }
    return bins;
}

QVector<int> DiagnosticsLatencyChart::sparklineSamples(int maxPoints) const {
    QVector<int> out;
    if (samples_.isEmpty() || maxPoints <= 0) {
        return out;
    }

    const int start = qMax(0, samples_.size() - maxPoints);
    out.reserve(samples_.size() - start);
    for (int i = start; i < samples_.size(); ++i) {
        out.append(samples_.at(i));
    }
    return out;
}

qreal DiagnosticsLatencyChart::mapY(int latencyUs, qreal yMax,
                                    const QRect& plot) const {
    const qreal clamped = qBound(0.0, static_cast<qreal>(latencyUs), yMax);
    return plot.bottom() - (clamped / yMax) * plot.height();
}

void DiagnosticsLatencyChart::paintHealthCard(QPainter& painter, const QRect& area,
                                              const HealthSummary& health) {
    painter.setPen(cardBorderColor_);
    painter.setBrush(cardFillColor_);
    painter.drawRoundedRect(area, 4, 4);

    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    painter.setFont(titleFont);
    painter.setPen(health.labelColor);
    painter.drawText(area.adjusted(10, 8, -10, -52), Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("HEALTH: %1").arg(health.label));

    painter.setFont(font());
    const int lineHeight = 16;
    int y = area.top() + 34;
    auto drawLine = [&](const QString& label, const QString& value) {
        painter.setPen(mutedTextColor_);
        painter.drawText(area.left() + 10, y, label);
        painter.setPen(textColor_);
        painter.drawText(area.right() - 10 - painter.fontMetrics().horizontalAdvance(value),
                         y, value);
        y += lineHeight;
    };

    if (chartMode_ == ChartMode::WireRtt) {
        drawLine(QStringLiteral("Recv Timeouts:"),
                 QString::number(health.recvTimeouts));
        drawLine(QStringLiteral("Reply Rejects:"),
                 QString::number(replyRejects_));
        drawLine(QStringLiteral("Exchanges:"),
                 QStringLiteral("%1 / %2")
                     .arg(health.exchangeReplies)
                     .arg(health.cyclesCompleted));
    } else {
        drawLine(QStringLiteral("Stale Frames:"), QString::number(health.staleFrames));
        drawLine(QStringLiteral("Duplicate Frames:"),
                 QString::number(health.duplicateFrames));
    }
    if (chartMode_ == ChartMode::WireRtt) {
        if (health.worstLatencyUs > 0) {
            drawLine(QStringLiteral("Worst Wire RTT:"),
                     QStringLiteral("%1 µs").arg(health.worstLatencyUs));
        } else {
            drawLine(QStringLiteral("Worst Wire RTT:"), QStringLiteral("—"));
        }
    } else if (health.worstLatencyUs > 0) {
        drawLine(QStringLiteral("Worst Latency:"),
                 QStringLiteral("%1 µs").arg(health.worstLatencyUs));
    } else {
        drawLine(QStringLiteral("Worst Latency:"), QStringLiteral("—"));
    }
}

void DiagnosticsLatencyChart::paintSparkline(QPainter& painter, const QRect& area,
                                             const QVector<int>& samples,
                                             const QColor& color) const {
    if (area.width() < 4 || area.height() < 4 || samples.size() < 2) {
        return;
    }

    int minV = samples.constFirst();
    int maxV = samples.constFirst();
    for (int sample : samples) {
        minV = qMin(minV, sample);
        maxV = qMax(maxV, sample);
    }
    const int span = qMax(1, maxV - minV);

    QPainterPath path;
    for (int i = 0; i < samples.size(); ++i) {
        const qreal x =
            area.left() + (static_cast<qreal>(i) / (samples.size() - 1)) * area.width();
        const qreal yNorm = static_cast<qreal>(samples.at(i) - minV) / span;
        const qreal y = area.bottom() - yNorm * area.height();
        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    painter.setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
}

void DiagnosticsLatencyChart::paintStatCards(QPainter& painter, const QRect& area,
                                             const LatencyStats& stats) {
    struct CardDef {
        QString label;
        QString value;
        const QVector<int>* spark = nullptr;
        bool thresholdColor;
        int thresholdMicros;
    };

    const QString noData = QStringLiteral("—");
    const bool hasData = stats.count > 0;
    const bool wireRtt = chartMode_ == ChartMode::WireRtt;
    const QVector<int> sparkRaw = sparklineSamples(kSparklinePoints);
    const QVector<int> sparkAvg =
        rollingAvgSparkline(samples_, kRollingWindow, kSparklinePoints);
    const QVector<int> sparkMin =
        rollingMinSparkline(samples_, kRollingWindow, kSparklinePoints);
    const QVector<int> sparkStd =
        rollingStdSparkline(samples_, kRollingWindow, kSparklinePoints);

    const CardDef wireCards[] = {
        {QStringLiteral("Last (\u00b5s)"),
         hasData ? QString::number(stats.lastUs) : noData,
         &sparkRaw,
         false,
         0},
        {QStringLiteral("Avg (\u00b5s)"),
         hasData ? QString::number(qRound(stats.avgUs)) : noData,
         &sparkAvg,
         false,
         0},
        {QStringLiteral("Min (\u00b5s)"),
         hasData ? QString::number(stats.minUs) : noData,
         &sparkMin,
         false,
         0},
        {QStringLiteral("Max (\u00b5s)"),
         hasData ? QString::number(stats.maxUs) : noData,
         &sparkRaw,
         true,
         stats.maxUs},
        {QStringLiteral("Std (\u00b5s)"),
         hasData ? QString::number(qRound(stats.stdUs)) : noData,
         &sparkStd,
         false,
         0},
    };
    const CardDef replyCards[] = {
        {QStringLiteral("Last (\u00b5s)"),
         hasData ? QString::number(stats.lastUs) : noData,
         &sparkRaw,
         false,
         0},
        {QStringLiteral("Avg (\u00b5s)"),
         hasData ? QString::number(qRound(stats.avgUs)) : noData,
         &sparkAvg,
         false,
         0},
        {QStringLiteral("P99 (\u00b5s)"),
         hasData ? QString::number(stats.p99Us) : noData,
         nullptr,
         true,
         stats.p99Us},
        {QStringLiteral("P99.9 (\u00b5s)"),
         hasData ? QString::number(stats.p999Us) : noData,
         nullptr,
         true,
         stats.p999Us},
        {QStringLiteral("Max (\u00b5s)"),
         hasData ? QString::number(stats.maxUs) : noData,
         &sparkRaw,
         true,
         stats.maxUs},
    };
    const CardDef* cards = wireRtt ? wireCards : replyCards;
    const int cardCount = 5;
    const int gap = 6;
    const int cardWidth = (area.width() - gap * (cardCount - 1)) / cardCount;
    const int cardHeight = area.height();

    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() - 0.5);
    QFont valueFont = font();
    valueFont.setBold(true);
    valueFont.setPointSizeF(valueFont.pointSizeF() + 1.0);

    for (int i = 0; i < cardCount; ++i) {
        const QRect cardRect(area.left() + i * (cardWidth + gap), area.top(),
                             cardWidth, cardHeight);
        painter.setPen(cardBorderColor_);
        painter.setBrush(cardFillColor_);
        painter.drawRoundedRect(cardRect, 4, 4);

        painter.setFont(labelFont);
        painter.setPen(mutedTextColor_);
        painter.drawText(cardRect.left() + 8, cardRect.top() + 18, cards[i].label);

        painter.setFont(valueFont);
        QColor valueColor = cardValueColor_;
        if (cards[i].thresholdColor && hasData) {
            valueColor = latencyStatColor(cards[i].thresholdMicros, chartMode_);
        }
        painter.setPen(valueColor);
        const QFontMetrics valueMetrics(valueFont);
        const int valueBaseline = cardRect.top() + 40 + valueMetrics.ascent();
        painter.drawText(cardRect.left() + 8, valueBaseline, cards[i].value);

        if (cards[i].spark != nullptr && !cards[i].spark->isEmpty()) {
            const QRect sparkRect(cardRect.left() + 6, cardRect.bottom() - 24,
                                  cardRect.width() - 12, 18);
            paintSparkline(painter, sparkRect, *cards[i].spark, rawLineColor_);
        }
    }
}

void DiagnosticsLatencyChart::paintLineChart(QPainter& painter, const QRect& plot,
                                             const QVector<double>& rollingAvg,
                                             int yMaxUs) {
    const qreal yMax = qMax(kLineYMaxUs, yMaxUs);
    const qreal xMin = static_cast<qreal>(baseExchange_);
    const qreal xMax =
        samples_.isEmpty()
            ? xMin
            : static_cast<qreal>(
                  baseExchange_ + static_cast<uint32_t>(samples_.size()) - 1u);
    const qreal xSpan = (xMax > xMin) ? (xMax - xMin) : 1.0;
    const QColor axisText(QStringLiteral("#444444"));

    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(axisText);
    painter.drawText(plot.adjusted(0, -20, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     trendTitle_);

    painter.fillRect(plot, plotBackground_);

    auto drawZone = [&](int lowUs, int highUs, const QColor& color) {
        const int yTop = static_cast<int>(mapY(highUs, yMax, plot));
        const int yBottom = static_cast<int>(mapY(lowUs, yMax, plot));
        painter.fillRect(plot.left(), yTop, plot.width(), yBottom - yTop, color);
    };

    drawZone(0, 250, zoneGreen_);
    drawZone(250, kWarnLatencyUs, zoneYellow_);
    drawZone(kWarnLatencyUs, static_cast<int>(yMax), zoneRed_);

    painter.setPen(gridColor_);
    painter.drawRect(plot);

    const int vTicks = 5;
    for (int i = 1; i < vTicks; ++i) {
        const int y = plot.top() + (plot.height() * i) / vTicks;
        painter.drawLine(plot.left(), y, plot.right(), y);
    }
    const int hTicks = 5;
    for (int i = 1; i < hTicks; ++i) {
        const int x = plot.left() + (plot.width() * i) / hTicks;
        painter.drawLine(x, plot.top(), x, plot.bottom());
    }

    QPen thresholdPen(thresholdColor_, 1.0, Qt::DashLine);
    painter.setFont(font());
    const QFontMetrics metrics(font());
    if (chartMode_ == ChartMode::WireRtt) {
        const int thresholds[] = {LEAP_CONF_IO_BENCH_MAX_RTT_US,
                                  LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US};
        for (int threshold : thresholds) {
            if (threshold <= 0 || threshold > static_cast<int>(yMax)) {
                continue;
            }
            const int y = static_cast<int>(mapY(threshold, yMax, plot));
            painter.setPen(thresholdPen);
            painter.drawLine(plot.left(), y, plot.right(), y);
            painter.setPen(thresholdColor_);
            painter.drawText(plot.right() + 4, y + metrics.ascent() / 2,
                             QString::number(threshold));
        }
    } else {
        for (int threshold : kThresholdsUs) {
            const int y = static_cast<int>(mapY(threshold, yMax, plot));
            painter.setPen(thresholdPen);
            painter.drawLine(plot.left(), y, plot.right(), y);
            painter.setPen(thresholdColor_);
            painter.drawText(plot.right() + 4, y + metrics.ascent() / 2,
                             QString::number(threshold));
        }
    }

    auto drawPath = [&](const QVector<double>& values, const QColor& color,
                        qreal width) {
        QPainterPath path;
        for (int i = 0; i < values.size(); ++i) {
            const qreal xNorm =
                (static_cast<qreal>(baseExchange_ + static_cast<uint32_t>(i)) - xMin) /
                xSpan;
            const qreal px = plot.left() + xNorm * plot.width();
            const qreal py = mapY(static_cast<int>(values.at(i)), yMax, plot);
            if (i == 0) {
                path.moveTo(px, py);
            } else {
                path.lineTo(px, py);
            }
        }
        painter.setPen(
            QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    };

    QVector<double> rawValues;
    rawValues.reserve(samples_.size());
    for (int sample : samples_) {
        rawValues.append(sample);
    }

    drawPath(rollingAvg, avgLineColor_, 2.5);
    drawPath(rawValues, rawLineColor_, 1.0);

    painter.setPen(axisText);
    painter.drawText(plot.adjusted(0, plot.height() + 2, 0, 18), Qt::AlignHCenter,
                     QStringLiteral("Exchange"));

    painter.save();
    painter.translate(10, plot.center().y());
    painter.rotate(-90.0);
    painter.drawText(QRect(-plot.height() / 2, -36, plot.height(), 20), Qt::AlignCenter,
                     QStringLiteral("µs"));
    painter.restore();

    painter.drawText(plot.left(), plot.bottom() + metrics.ascent() + 2,
                     QString::number(static_cast<qint64>(xMin)));
    painter.drawText(plot.right() - metrics.horizontalAdvance(QString::number(
                         static_cast<qint64>(xMax))),
                     plot.bottom() + metrics.ascent() + 2,
                     QString::number(static_cast<qint64>(xMax)));
    painter.drawText(4, plot.bottom(), QStringLiteral("0"));
    painter.drawText(4, plot.top() + metrics.ascent(),
                     QString::number(static_cast<int>(yMax)));

    const int legendY = plot.top() - 4;
    auto drawLegend = [&](int x, const QColor& color, qreal width,
                          const QString& label) {
        painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(x, legendY, x + 16, legendY);
        painter.setPen(axisText);
        painter.drawText(x + 20, legendY + 4, label);
    };
    drawLegend(plot.left(), rawLineColor_, 1.0, QStringLiteral("Raw"));
    drawLegend(plot.left() + 72, avgLineColor_, 2.5,
               QStringLiteral("%1-sample avg").arg(kRollingWindow));
}

void DiagnosticsLatencyChart::paintHistogram(QPainter& painter, const QRect& area,
                                             const QVector<int>& bins, int maxCount,
                                             const QVector<int>& binEdges,
                                             int axisMaxUs) {
    const QColor axisText(QStringLiteral("#444444"));
    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(axisText);
    const QString histTitle =
        chartMode_ == ChartMode::WireRtt
            ? QStringLiteral("Wire RTT Distribution")
            : QStringLiteral("Reply Latency Distribution");
    painter.drawText(QRect(area.left(), area.top(), area.width(), 18), Qt::AlignLeft,
                     histTitle);

    const QRect plot = area.adjusted(kMarginLeft, 22, -8, -22);
    if (plot.width() < 40 || plot.height() < 40 || maxCount <= 0) {
        painter.setFont(font());
        painter.setPen(mutedTextColor_);
        painter.drawText(plot, Qt::AlignCenter,
                         QStringLiteral("No distribution data"));
        return;
    }

    painter.fillRect(plot, plotBackground_);
    painter.setFont(font());
    painter.setPen(gridColor_);
    painter.drawRect(plot);

    const int barGap = 4;
    const int barWidth =
        qMax(6, (plot.width() - barGap * (bins.size() - 1)) / bins.size());
    const QFontMetrics metrics(font());

    for (int i = 0; i < bins.size(); ++i) {
        const int count = bins.at(i);
        const int barHeight =
            (count > 0) ? qMax(2, (count * (plot.height() - 2)) / maxCount) : 0;
        const int x = plot.left() + i * (barWidth + barGap);
        const int y = plot.bottom() - barHeight;

        const int binStartUs =
            (i < binEdges.size()) ? binEdges.at(i) : (i * axisMaxUs / bins.size());
        QColor barColor = histogramGreenBar_;
        if (chartMode_ == ChartMode::WireRtt) {
            if (binStartUs >= LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US) {
                barColor = histogramRedBar_;
            } else if (binStartUs >= LEAP_CONF_IO_BENCH_MAX_RTT_US) {
                barColor = histogramAmberBar_;
            }
        } else if (binStartUs >= 500) {
            barColor = histogramRedBar_;
        } else if (binStartUs >= 250) {
            barColor = histogramAmberBar_;
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor);
        painter.drawRect(x, y, barWidth, barHeight);
    }

    painter.setPen(axisText);
    const int labelY = plot.bottom() + metrics.ascent() + 4;
    const QVector<int> axisLabels =
        chartMode_ == ChartMode::WireRtt
            ? wireRttHistogramAxisLabels(axisMaxUs)
            : QVector<int>(std::begin(kHistogramAxisLabelsUs),
                           std::end(kHistogramAxisLabelsUs));
    const int histAxisMax =
        chartMode_ == ChartMode::WireRtt ? axisMaxUs : kLineYMaxUs;
    for (int labelUs : axisLabels) {
        const qreal xNorm = static_cast<qreal>(labelUs) / histAxisMax;
        const int x = plot.left() + static_cast<int>(xNorm * plot.width());
        const QString label =
            (labelUs == histAxisMax)
                ? QStringLiteral("%1 µs").arg(labelUs)
                : QString::number(labelUs);
        painter.drawText(x - metrics.horizontalAdvance(label) / 2, labelY, label);
    }

    painter.drawText(plot.left() - kMarginLeft + 4, plot.top() + metrics.ascent(),
                     QString::number(maxCount));
    painter.drawText(plot.left() - kMarginLeft + 4, plot.bottom(), QStringLiteral("0"));
}

void DiagnosticsLatencyChart::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), panelBackground_);

    const int pad = 4;
    const int contentTop = pad;
    const int contentBottom = height() - pad;
    const int contentHeight = contentBottom - contentTop;

    const QRect healthArea(width() - pad - kHealthCardWidth, contentTop,
                           kHealthCardWidth, kHealthCardHeight);
    const QRect statsArea(pad, healthArea.bottom() + kSectionGap,
                          width() - 2 * pad, kStatsCardsHeight);

    const int histFraction = 34;
    const int lineHeight =
        (contentHeight - kHealthCardHeight - kStatsCardsHeight - 2 * kSectionGap) *
        (100 - histFraction) / 100;
    const int histHeight = contentHeight - kHealthCardHeight - kStatsCardsHeight -
                           2 * kSectionGap - lineHeight;

    const QRect lineArea(pad, statsArea.bottom() + kSectionGap, width() - 2 * pad,
                         lineHeight);
    const QRect linePlot(lineArea.left() + kMarginLeft, lineArea.top() + 24,
                         lineArea.width() - kMarginLeft - kMarginRight,
                         lineArea.height() - 44);

    const QRect histArea(pad, lineArea.bottom() + kSectionGap, width() - 2 * pad,
                         histHeight);

    const LatencyStats stats = computeStats();
    const HealthSummary health = computeHealth(stats);
    paintHealthCard(painter, healthArea, health);
    paintStatCards(painter, statsArea, stats);

    const QVector<double> rollingAvg =
        samples_.isEmpty() ? QVector<double>() : computeRollingAverage();
    int histMax = 0;
    const QVector<int> bins =
        samples_.isEmpty() ? QVector<int>() : computeHistogram(&histMax);
    const int lineYMax =
        chartMode_ == ChartMode::WireRtt
            ? niceWireRttChartYMaxUs(stats.maxUs)
            : kLineYMaxUs;
    const QVector<int> histBinEdges =
        chartMode_ == ChartMode::WireRtt
            ? wireRttHistogramBinEdges(lineYMax)
            : QVector<int>(std::begin(kHistBinEdges), std::end(kHistBinEdges));
    const int histAxisMax =
        chartMode_ == ChartMode::WireRtt ? lineYMax : kLineYMaxUs;

    paintLineChart(painter, linePlot, rollingAvg, lineYMax);
    if (samples_.isEmpty()) {
        painter.setPen(mutedTextColor_);
        painter.drawText(linePlot, Qt::AlignCenter,
                         QStringLiteral("No exchange data yet"));
    }
    paintHistogram(painter, histArea, bins, histMax, histBinEdges, histAxisMax);
}
