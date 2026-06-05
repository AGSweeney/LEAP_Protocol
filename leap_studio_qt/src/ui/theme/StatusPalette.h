#pragma once

#include <QColor>
#include <QString>

namespace leap::studio::theme {

inline QColor statusPass() { return QColor(0x3d, 0xba, 0x6c); }
inline QColor statusFail() { return QColor(0xd6, 0x45, 0x45); }
inline QColor statusWarn() { return QColor(0xe6, 0xa8, 0x00); }
inline QColor statusNeutral() { return QColor(0x88, 0x88, 0x88); }

inline QColor stateOp() { return statusPass(); }
inline QColor stateSafe() { return statusWarn(); }
inline QColor stateFault() { return statusFail(); }
inline QColor stateOther() { return statusNeutral(); }

inline QColor conformanceStatusColor(const QString& status) {
    if (status == QStringLiteral("PASS")) {
        return statusPass();
    }
    if (status == QStringLiteral("FAIL")) {
        return statusFail();
    }
    if (status == QStringLiteral("SKIP") || status == QStringLiteral("WARN")) {
        return statusWarn();
    }
    return QColor();
}

}  // namespace leap::studio::theme
