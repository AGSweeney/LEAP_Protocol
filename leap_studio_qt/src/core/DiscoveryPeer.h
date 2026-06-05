#pragma once

#include <QString>

struct DiscoveryPeerRow {
    QString mac;
    QString platform;
    QString product;
    QString profile;
    QString state;
    uint16_t stateCode = 0;
    QString leapVersion;
    QString fw;
    QString vendor;
};
