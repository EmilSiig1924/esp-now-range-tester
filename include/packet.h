#pragma once
#include <cstdint>

// Both ESP32 targets are little-endian. Fixed-width fields keep the wire layout
// identical. A random session ID changes on reboot so loss totals can restart.
struct TestPacket {
    uint32_t magic;
    uint32_t version;
    uint32_t session;
    uint32_t sequence;
    uint32_t uptimeMs;
};
static_assert(sizeof(TestPacket) == 20, "Packet wire layout must stay stable");
