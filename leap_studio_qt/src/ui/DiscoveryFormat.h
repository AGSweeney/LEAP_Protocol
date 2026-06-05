#pragma once

#include <QColor>
#include <QString>

#include <QtGlobal>

namespace leap::studio::discovery {

QString stateName(uint16_t state);
QColor stateColor(uint16_t state);
QString platformName(uint32_t productCode);
QString productName(uint32_t productCode);
QString vendorName(uint16_t vendorId, uint32_t productCode = 0u);
QString profileText(uint32_t profileId);
QString leapProtocolText();
QString formatLastSeenAgo(qint64 seenEpochMs);
QString stateWithLastSeen(const QString& state, qint64 seenEpochMs);
QString normalizeMacKey(const QString& mac);

}  // namespace leap::studio::discovery
