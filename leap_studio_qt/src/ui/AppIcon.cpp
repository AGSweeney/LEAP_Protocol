#include "ui/AppIcon.h"

#include <QSize>

namespace leap::studio {

QIcon loadAppIcon() {
    QIcon icon;
    const auto add = [&icon](const char* path, int size) {
        icon.addFile(QString::fromUtf8(path), QSize(size, size));
    };

    add(":/icons/leap_studio_16.png", 16);
    add(":/icons/leap_studio_24.png", 24);
    add(":/icons/leap_studio_32.png", 32);
    add(":/icons/leap_studio_48.png", 48);
    add(":/icons/leap_studio_64.png", 64);
    add(":/icons/leap_studio_128.png", 128);
    add(":/icons/leap_studio_256.png", 256);
    return icon;
}

}  // namespace leap::studio
