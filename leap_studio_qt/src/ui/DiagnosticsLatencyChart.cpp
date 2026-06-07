#include "ui/DiagnosticsLatencyChart.h"

#include "ui/theme/StatusPalette.h"

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

QColor latencyStatColor(int micros) {
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

void DiagnosticsLatencyChart::clearTrend() {
    samples_.clear();
    trendTitle_ = QStringLiteral("Reply Latency Trend");
    baseExchange_ = 0u;
    staleFrames_ = 0u;
    duplicateFrames_ = 0u;
    update();
}

void DiagnosticsLatencyChart::applyMetrics(const LeapConformanceMetrics& metrics) {
    staleFrames_ = metrics.stale_frames;
    duplicateFrames_ = metrics.duplicate_frames;

    const bool useWireRtt =
        metrics.network_rtt_trend.count > 0u &&
        metrics.network_rtt_trend.count >= metrics.reply_latency_trend.count;
    const LeapConformanceLatencyTrend& trend =
        useWireRtt ? metrics.network_rtt_trend : metrics.reply_latency_trend;

    trendTitle_ = useWireRtt ? QStringLiteral("Wire RTT (soak)")
                             : QStringLiteral("Reply Latency Trend");

    if (trend.count == 0u && !samples_.isEmpty()) {
        update();
        return;
    }

    samples_.resize(static_cast<int>(trend.count));
    for (uint32_t i = 0u; i < trend.count; i++) {
        samples_[static_cast<int>(i)] = static_cast<int>(trend.samples[i]);
    }
    baseExchange_ = trend.base_exchange;
    update();
}

DiagnosticsLatencyChart::LatencyStats DiagnosticsLatencyChart::computeStats() const {
    LatencyStats stats;
    if (samples_.isEmpty()) {
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
    health.worstLatencyUs = stats.maxUs;

    const bool hasIssues =
        staleFrames_ > 0u || duplicateFrames_ > 0u ||
        (stats.count > 0 && stats.maxUs >= kWarnLatencyUs);
    const bool severe =
        staleFrames_ > 0u || duplicateFrames_ > 0u ||
        (stats.count > 0 && stats.maxUs >= 1000);

    if (stats.count == 0) {
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
    QVector<int> bins(kHistBinCount, 0);
    int maxCount = 0;

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

    drawLine(QStringLiteral("Stale Frames:"), QString::number(health.staleFrames));
    drawLine(QStringLiteral("Duplicate Frames:"),
             QString::number(health.duplicateFrames));
    if (health.worstLatencyUs > 0) {
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
        bool sparkline;
        bool thresholdColor;
        int thresholdMicros;
    };

    const QString noData = QStringLiteral("—");
    const bool hasData = stats.count > 0;
    const CardDef cards[] = {
        {QStringLiteral("Last (\u00b5s)"),
         hasData ? QString::number(stats.lastUs) : noData,
         true,
         false,
         0},
        {QStringLiteral("Avg (\u00b5s)"),
         hasData ? QString::number(qRound(stats.avgUs)) : noData,
         true,
         false,
         0},
        {QStringLiteral("P99 (\u00b5s)"),
         hasData ? QString::number(stats.p99Us) : noData,
         false,
         true,
         stats.p99Us},
        {QStringLiteral("P99.9 (\u00b5s)"),
         hasData ? QString::number(stats.p999Us) : noData,
         false,
         true,
         stats.p999Us},
        {QStringLiteral("Max (\u00b5s)"),
         hasData ? QString::number(stats.maxUs) : noData,
         true,
         true,
         stats.maxUs},
    };

    const QVector<int> spark = sparklineSamples(kSparklinePoints);
    const int cardCount = static_cast<int>(sizeof(cards) / sizeof(cards[0]));
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
            valueColor = latencyStatColor(cards[i].thresholdMicros);
        }
        painter.setPen(valueColor);
        const QFontMetrics valueMetrics(valueFont);
        const int valueBaseline = cardRect.top() + 40 + valueMetrics.ascent();
        painter.drawText(cardRect.left() + 8, valueBaseline, cards[i].value);

        if (cards[i].sparkline && !spark.isEmpty()) {
            const QRect sparkRect(cardRect.left() + 6, cardRect.bottom() - 24,
                                  cardRect.width() - 12, 18);
            paintSparkline(painter, sparkRect, spark, rawLineColor_);
        }
    }
}

void DiagnosticsLatencyChart::paintLineChart(QPainter& painter, const QRect& plot,
                                             const QVector<double>& rollingAvg) {
    const qreal yMax = kLineYMaxUs;
    const qreal xMin = static_cast<qreal>(baseExchange_);
    const qreal xMax =
        static_cast<qreal>(baseExchange_ + static_cast<uint32_t>(samples_.size()) - 1u);
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
    for (int threshold : kThresholdsUs) {
        const int y = static_cast<int>(mapY(threshold, yMax, plot));
        painter.setPen(thresholdPen);
        painter.drawLine(plot.left(), y, plot.right(), y);
        painter.setPen(thresholdColor_);
        painter.drawText(plot.right() + 4, y + metrics.ascent() / 2,
                         QString::number(threshold));
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
                                             const QVector<int>& bins, int maxCount) {
    const QColor axisText(QStringLiteral("#444444"));
    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(axisText);
    painter.drawText(QRect(area.left(), area.top(), area.width(), 18), Qt::AlignLeft,
                     QStringLiteral("Reply Latency Distribution"));

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

        const int binStartUs = kHistBinEdges[i];
        QColor barColor = histogramGreenBar_;
        if (binStartUs >= 500) {
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
    for (int labelUs : kHistogramAxisLabelsUs) {
        const qreal xNorm = static_cast<qreal>(labelUs) / kLineYMaxUs;
        const int x = plot.left() + static_cast<int>(xNorm * plot.width());
        const QString label =
            (labelUs == kLineYMaxUs)
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

    if (samples_.isEmpty()) {
        painter.setPen(mutedTextColor_);
        painter.drawText(linePlot, Qt::AlignCenter,
                         QStringLiteral("No exchange data yet"));
        return;
    }

    const QVector<double> rollingAvg = computeRollingAverage();
    int histMax = 0;
    const QVector<int> bins = computeHistogram(&histMax);

    paintLineChart(painter, linePlot, rollingAvg);
    paintHistogram(painter, histArea, bins, histMax);
}
