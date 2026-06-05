#pragma once

#include <QString>
#include <QVector>

extern "C" {
#include "leap/conformance/leap_conformance_metrics.h"
}

namespace leap::studio::diagnostics {

QString formatMac(const uint8_t mac[6]);
QString formatDurationUs(uint32_t micros);
QString formatDurationUs64(uint64_t micros);
QString formatFrameCount(uint64_t count, int fromDevice);

struct DiagnosticsTableRow {
    enum class Kind { Section, Field };
    Kind kind = Kind::Field;
    QString label;
    QString value;
};

struct DiagnosticsTrafficRates {
    double rxFps = 0.0;
    double txFps = 0.0;
    bool hasRates = false;
};

void populateDiagnosticsRows(const LeapConformanceMetrics& metrics,
                             QVector<DiagnosticsTableRow>* rows,
                             const DiagnosticsTrafficRates& rates = {});

}  // namespace leap::studio::diagnostics


