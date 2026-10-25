#include "lamp.h"

constexpr bool idleAndSingleFlash() {
    LampBlinker lamp;
    if (lamp.lit(0)) return false; // Off until a pattern starts.
    lamp.start(1, 400000, 0, 1000);
    if (lamp.lit(999) || !lamp.lit(1000) || !lamp.lit(400999)) return false;
    return !lamp.lit(401000);
}

constexpr bool repeatedFlashes() {
    LampBlinker lamp;
    lamp.start(3, 100000, 100000, 0);
    if (!lamp.lit(0) || !lamp.lit(99999) || lamp.lit(100000) || lamp.lit(199999)) return false;
    if (!lamp.lit(200000) || !lamp.lit(400000) || !lamp.lit(499999)) return false;
    return !lamp.lit(500000) && !lamp.lit(600000) && !lamp.lit(10000000);
}

constexpr bool restartReplacesPattern() {
    LampBlinker lamp;
    lamp.start(5, 50000, 50000, 0);
    lamp.start(1, 400000, 0, 60000); // Would be dark at 60 ms under the old pattern.
    return lamp.lit(60000) && !lamp.lit(460000);
}

static_assert(idleAndSingleFlash(), "Lamp stays dark until started and ends on time");
static_assert(repeatedFlashes(), "Flash count and on/off timing");
static_assert(restartReplacesPattern(), "A new event replaces the running pattern");
