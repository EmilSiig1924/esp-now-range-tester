#pragma once
#include <cstdint>
#include <limits>

enum class SensorStatus : uint32_t {
    notReady = 0,
    ok = 1,
    noEcho = 2,
    outOfRange = 3,
    error = 4,
    stale = 5,
};
constexpr uint32_t unknownSensorAge = std::numeric_limits<uint32_t>::max();

constexpr const char* sensorStatusName(SensorStatus status) {
    switch (status) {
    case SensorStatus::notReady: return "not_ready";
    case SensorStatus::ok: return "ok";
    case SensorStatus::noEcho: return "no_echo";
    case SensorStatus::outOfRange: return "out_of_range";
    case SensorStatus::error: return "error";
    case SensorStatus::stale: return "stale";
    }
    return "unknown";
}

struct DistanceReading {
    SensorStatus status = SensorStatus::notReady;
    uint32_t distanceMm = 0; // Valid only when status == ok.
};

// Approximate speed of sound in air at 20 C: 343 m/s. Divide the round-trip
// pulse time by two. The documented AJ-SR04M mode-1 envelope is 0.2 to 8 m.
constexpr DistanceReading distanceFromPulseUs(uint32_t pulseUs) {
    if (pulseUs == 0) return {SensorStatus::noEcho, 0};
    const uint64_t mm = (static_cast<uint64_t>(pulseUs) * 343 + 1000) / 2000;
    if (mm < 200 || mm > 8000) return {SensorStatus::outOfRange, 0};
    return {SensorStatus::ok, static_cast<uint32_t>(mm)};
}

constexpr SensorStatus sensorStatusForAge(SensorStatus status, uint32_t ageMs,
                                         uint32_t staleAfterMs) {
    return status == SensorStatus::ok && ageMs >= staleAfterMs ? SensorStatus::stale : status;
}
