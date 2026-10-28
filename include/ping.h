#pragma once
#include <cstdint>
#include "sensor_data.h"

// Separate protocol from the original continuous range test. Both ESP32s use
// little-endian fixed-width fields; no pointers or local timestamps on the wire.
constexpr uint32_t pingMagic = 0x50494E47;
constexpr uint32_t pingVersion = 2; // Sensor fields added: update BOTH boards.
enum class PingType : uint32_t { request = 1, reply = 2 };

struct PingPacket {
    uint32_t magic = pingMagic;
    uint32_t version = pingVersion;
    PingType type = PingType::request;
    uint32_t session = 0;
    uint32_t sequence = 0;
    int32_t outwardRssi = 0;
    uint32_t remoteTurnaroundUs = 0;
    SensorStatus sensorStatus = SensorStatus::notReady;
    uint32_t distanceMm = 0;
    uint32_t sensorAgeMs = unknownSensorAge;
};
static_assert(sizeof(PingPacket) == 40, "Ping wire layout must stay stable");

// Require a stable change before reporting a press. Initializing from the
// actual pin level prevents a button held during boot from triggering a ping.
struct ButtonDebouncer {
    bool rawPressed;
    bool stablePressed;
    int64_t changedAtUs;

    constexpr bool update(bool pressed, int64_t nowUs, int64_t debounceUs) {
        if (pressed != rawPressed) {
            rawPressed = pressed;
            changedAtUs = nowUs;
        }
        if (rawPressed == stablePressed || nowUs - changedAtUs < debounceUs) return false;
        stablePressed = rawPressed;
        return stablePressed;
    }
};

// Each press toggles continuous pinging. While running, a request is due
// immediately after starting, then one interval after the previous request,
// but never while an earlier request still awaits its reply or timeout.
struct PingSchedule {
    bool running = false;
    bool sentThisRun = false;
    int64_t lastSentUs = 0;

    constexpr bool toggle() {
        running = !running;
        sentThisRun = false;
        return running;
    }
    constexpr bool due(int64_t nowUs, bool awaitingReply, int64_t intervalUs) const {
        return running && !awaitingReply && (!sentThisRun || nowUs - lastSentUs >= intervalUs);
    }
    constexpr void sent(int64_t nowUs) {
        sentThisRun = true;
        lastSentUs = nowUs;
    }
};

struct PendingPing {
    bool active = false;
    uint32_t session = 0;
    uint32_t sequence = 0;
    int64_t startedAtUs = 0;

    constexpr bool matches(const PingPacket& packet, int64_t arrivedAtUs,
                           int64_t timeoutUs) const {
        return active && packet.magic == pingMagic && packet.version == pingVersion &&
               packet.type == PingType::reply && packet.session == session &&
               packet.sequence == sequence && arrivedAtUs >= startedAtUs &&
               arrivedAtUs - startedAtUs < timeoutUs;
    }
};
