#include "ui/AppIcon.h"
#include "ui/MainWindow.h"
#include "ui/theme/ThemeManager.h"
#include "ui/theme/ThemeTokens.h"

#include <QApplication>
#include <QFont>
#include <QMetaType>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

extern "C" {
#include "leap/conformance/leap_conformance_metrics.h"
}

Q_DECLARE_METATYPE(LeapConformanceMetrics)

int main(int argc, char* argv[]) {
    qRegisterMetaType<LeapConformanceMetrics>("LeapConformanceMetrics");

    QApplication app(argc, argv);
    QApplication::setApplicationName(leap::studio::theme::kAppName);
    QApplication::setOrganizationName(leap::studio::theme::kOrgName);
    QApplication::setApplicationDisplayName(
        QStringLiteral("LEAP Conformance Studio"));
    app.setWindowIcon(leap::studio::loadAppIcon());

    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }

    QFont appFont(QStringLiteral("Segoe UI"));
    appFont.setPixelSize(12);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(appFont);

    QSettings settings(leap::studio::theme::kOrgName,
                       leap::studio::theme::kAppName);
    const QString theme =
        settings.value(leap::studio::theme::kSettingsThemeKey, QStringLiteral("dark"))
            .toString();
    leap::studio::theme::ThemeManager::applyTheme(app, theme);

    MainWindow window;
    window.show();

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--auto-bench") == 0) {
            QTimer::singleShot(1200, &window, [&window]() {
                window.runAutoBenchDemo(10u);
            });
            break;
        }
    }

    return app.exec();
}
