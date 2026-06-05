#include "ui/theme/ThemeManager.h"

#include "ui/theme/ComboPopupItemDelegate.h"
#include "ui/theme/ThemeTokens.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QPalette>

namespace leap::studio::theme {

QString ThemeManager::loadStylesheet(const QString& themeId) {
    const QString fileName =
        themeId == QLatin1String("light") ? QStringLiteral("studio_light.qss")
                                          : QStringLiteral("studio_dark.qss");
    QFile file(QStringLiteral(":/themes/") + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFile local(QStringLiteral("themes/") + fileName);
        if (local.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(local.readAll());
        }
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

void ThemeManager::applyPalette(QApplication& app, const QString& themeId) {
    QPalette palette = app.palette();
    if (themeId == QLatin1String("light")) {
        palette.setColor(QPalette::Window, QColor("#f0f0f0"));
        palette.setColor(QPalette::WindowText, QColor("#1a1a1a"));
        palette.setColor(QPalette::Highlight, QColor(kLightAccent));
    } else {
        palette.setColor(QPalette::Window, QColor(kDarkChrome));
        palette.setColor(QPalette::WindowText, QColor(kDarkText));
        palette.setColor(QPalette::Base, QColor(kDarkInput));
        palette.setColor(QPalette::AlternateBase, QColor(kDarkRowAlt));
        palette.setColor(QPalette::Highlight, QColor(kDarkAccent));
        palette.setColor(QPalette::HighlightedText, QColor(kDarkText));
    }
    app.setPalette(palette);
}

void ThemeManager::styleComboBoxPopup(QComboBox* combo) {
    if (combo == nullptr) {
        return;
    }
    combo->setItemDelegate(new ComboPopupItemDelegate(combo));
}

void ThemeManager::applyTheme(QApplication& app, const QString& themeId) {
    applyPalette(app, themeId);
    const QString qss = loadStylesheet(themeId);
    if (!qss.isEmpty()) {
        app.setStyleSheet(qss);
    }
}

}  // namespace leap::studio::theme
