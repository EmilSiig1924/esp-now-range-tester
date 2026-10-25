#pragma once
#include <cstdint>

// Non-blocking flash pattern for the button lamp. The main loop polls lit()
// each pass, so pattern timing never delays radio handling or RTT measurement.
struct LampBlinker {
    uint32_t flashes = 0; // Zero means no pattern is running.
    int64_t onUs = 0;
    int64_t offUs = 0;
    int64_t startedAtUs = 0;

    constexpr void start(uint32_t count, int64_t flashOnUs, int64_t flashOffUs, int64_t nowUs) {
        flashes = count;
        onUs = flashOnUs;
        offUs = flashOffUs;
        startedAtUs = nowUs;
    }

    constexpr bool lit(int64_t nowUs) const {
        if (!flashes || onUs <= 0 || nowUs < startedAtUs) return false;
        const int64_t period = onUs + offUs;
        const int64_t elapsedUs = nowUs - startedAtUs;
        if (elapsedUs >= period * flashes) return false;
        return elapsedUs % period < onUs;
    }
};
