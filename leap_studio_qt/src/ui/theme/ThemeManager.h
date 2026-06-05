#pragma once

#include <QString>

class QApplication;
class QComboBox;

namespace leap::studio::theme {

class ThemeManager {
public:
    static void applyTheme(QApplication& app, const QString& themeId);
    static void styleComboBoxPopup(QComboBox* combo);
    static QString loadStylesheet(const QString& themeId);
    static void applyPalette(QApplication& app, const QString& themeId);
};

}  // namespace leap::studio::theme
