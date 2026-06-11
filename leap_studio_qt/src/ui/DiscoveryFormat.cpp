#include "ui/DiscoveryFormat.h"

#include "ui/theme/StatusPalette.h"

#include <QDateTime>

extern "C" {
#include "leap/leap_protocol.h"
}

namespace leap::studio::discovery {

QString stateName(uint16_t state) {
    switch (state) {
    case LEAP_STATE_BOOT:
        return QStringLiteral("BOOT");
    case LEAP_STATE_INIT:
        return QStringLiteral("INIT");
    case LEAP_STATE_CONFIGURED:
        return QStringLiteral("CONFIGURED");
    case LEAP_STATE_SAFE:
        return QStringLiteral("SAFE");
    case LEAP_STATE_OP:
        return QStringLiteral("OP");
    case LEAP_STATE_FAULT:
        return QStringLiteral("FAULT");
    default:
        return QStringLiteral("0x%1").arg(state, 4, 16, QChar('0'));
    }
}

QColor stateColor(uint16_t state) {
    switch (state) {
    case LEAP_STATE_SAFE:
        return leap::studio::theme::stateSafe();
    case LEAP_STATE_OP:
        return leap::studio::theme::stateOp();
    case LEAP_STATE_FAULT:
        return leap::studio::theme::stateFault();
    case LEAP_STATE_INIT:
    case LEAP_STATE_BOOT:
    case LEAP_STATE_CONFIGURED:
    default:
        return leap::studio::theme::stateOther();
    }
}

QString platformName(uint32_t productCode) {
    if (productCode == 0x0618C618u || productCode == 0x0868A016u) {
        return QStringLiteral("ESP32");
    }
    if (productCode == 0x434C4301u) {
        return QStringLiteral("ClearCore");
    }
    return QStringLiteral("—");
}

QString productName(uint32_t productCode) {
    if (productCode == 0x0618C618u) {
        return QStringLiteral("GL-C-618WL");
    }
    if (productCode == 0x0868A016u) {
        return QStringLiteral("KC868-A16");
    }
    if (productCode == 0x434C4301u) {
        return QStringLiteral("ClearCore LEAP");
    }
    return QStringLiteral("0x%1").arg(productCode, 8, 16, QChar('0'));
}

QString leapProtocolText() {
    return QStringLiteral("%1.%2")
        .arg(LEAP_VERSION_MAJOR)
        .arg(LEAP_VERSION_MINOR);
}

QString formatLastSeenAgo(qint64 seenEpochMs) {
    if (seenEpochMs <= 0) {
        return QStringLiteral("—");
    }

    const qint64 deltaMs = QDateTime::currentMSecsSinceEpoch() - seenEpochMs;
    if (deltaMs < 0) {
        return QStringLiteral("just now");
    }
    if (deltaMs < 1000) {
        return QStringLiteral("%1 ms ago").arg(deltaMs);
    }
    if (deltaMs < 60000) {
        return QStringLiteral("%1 s ago").arg(deltaMs / 1000);
    }
    if (deltaMs < 3600000) {
        return QStringLiteral("%1 min ago").arg(deltaMs / 60000);
    }
    return QStringLiteral("%1 h ago").arg(deltaMs / 3600000);
}

QString stateWithLastSeen(const QString& state, qint64 seenEpochMs) {
    const QString ago = formatLastSeenAgo(seenEpochMs);
    if (ago == QStringLiteral("—")) {
        return state;
    }
    return QStringLiteral("%1 | %2").arg(state, ago);
}

QString vendorName(uint16_t vendorId, uint32_t productCode) {
    if (vendorId == 0x544Bu) {
        return QStringLiteral("Teknic");
    }
    if (vendorId == 0u) {
        if (productCode == 0x0618C618u || productCode == 0x0868A016u) {
            return QStringLiteral("AGS");
        }
        return QStringLiteral("—");
    }

    const char hi = static_cast<char>((vendorId >> 8) & 0xFFu);
    const char lo = static_cast<char>(vendorId & 0xFFu);
    if (hi >= 'A' && hi <= 'Z' && lo >= 'A' && lo <= 'Z') {
        return QString(QLatin1Char(hi)) + QLatin1Char(lo) + QLatin1Char('S');
    }
    if (hi >= 'A' && hi <= 'Z' && lo >= '0' && lo <= '9') {
        return QString(QLatin1Char(hi)) + QLatin1Char(lo);
    }

    return QStringLiteral("0x%1").arg(vendorId, 4, 16, QChar('0'));
}

QString profileText(uint32_t profileId) {
    return QStringLiteral("0x%1").arg(profileId, 8, 16, QChar('0'));
}

QString normalizeMacKey(const QString& mac) {
    return mac.trimmed().toLower();
}

}  // namespace leap::studio::discovery
