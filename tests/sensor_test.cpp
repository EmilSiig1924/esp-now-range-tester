#include "sensor_data.h"

constexpr bool distanceAndInvalidReadings() {
    const auto near = distanceFromPulseUs(1166);
    if (near.status != SensorStatus::ok || near.distanceMm != 200) return false;
    const auto oneMetre = distanceFromPulseUs(5831);
    if (oneMetre.status != SensorStatus::ok || oneMetre.distanceMm != 1000) return false;
    const auto far = distanceFromPulseUs(46647);
    if (far.status != SensorStatus::ok || far.distanceMm != 8000) return false;
    if (distanceFromPulseUs(0).status != SensorStatus::noEcho) return false;
    const auto tooClose = distanceFromPulseUs(1000);
    if (tooClose.status != SensorStatus::outOfRange || tooClose.distanceMm != 0) return false;
    const auto tooFar = distanceFromPulseUs(60000);
    if (tooFar.status != SensorStatus::outOfRange || tooFar.distanceMm != 0) return false;
    return distanceFromPulseUs(0xffffffffu).status == SensorStatus::outOfRange;
}

constexpr bool staleAndStartupReadings() {
    if (sensorStatusForAge(SensorStatus::ok, 999, 1000) != SensorStatus::ok) return false;
    if (sensorStatusForAge(SensorStatus::ok, 1000, 1000) != SensorStatus::stale) return false;
    if (sensorStatusForAge(SensorStatus::notReady, unknownSensorAge, 1000) != SensorStatus::notReady) return false;
    return sensorStatusForAge(SensorStatus::noEcho, 1000, 1000) == SensorStatus::noEcho;
}

static_assert(distanceAndInvalidReadings(), "Distance conversion, blind zone, range, and overflow");
static_assert(staleAndStartupReadings(), "Do not present stale or startup readings as a valid distance");
