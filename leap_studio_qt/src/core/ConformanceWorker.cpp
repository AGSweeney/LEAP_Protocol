#include "core/ConformanceWorker.h"

extern "C" {
#include "leap_conformance_win_io.h"
}

ConformanceWorker::ConformanceWorker(QObject* parent) : QObject(parent) {
    ctx_ = leap_conformance_win_create();
    io_ = leap_conformance_win_io(ctx_);  /* const view; io() returns mutable for engine */
}

ConformanceWorker::~ConformanceWorker() {
    leap_conformance_win_destroy(ctx_);
    ctx_ = nullptr;
    io_ = nullptr;
}

LeapConformanceIo* ConformanceWorker::io() {
    return const_cast<LeapConformanceIo*>(io_);
}

void ConformanceWorker::setPeerMac(const uint8_t* mac, int hasMac) {
    leap_conformance_win_set_peer_mac(ctx_, mac, hasMac);
}
