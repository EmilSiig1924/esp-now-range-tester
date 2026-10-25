#include "statistics.h"

// Compile-time tests exercise the same logic used by the receiver. No board or
// test framework is needed: compile with a C++17 compiler and -fsyntax-only.
constexpr bool gapsAndDuplicates() {
    SequenceStats stats;
    if (stats.gapPercent() != 0.0) return false;
    stats.accept(1, 100); // No penalty for packets before the first observation.
    stats.accept(1, 101);
    stats.accept(1, 104);
    if (stats.received != 3 || stats.gaps != 2 || stats.gapPercent() != 40.0) return false;
    if (stats.accept(1, 104) || stats.accept(1, 103)) return false;
    return stats.lateOrDuplicate == 2 && stats.gaps == 2 && stats.received == 3;
}
constexpr bool rebootResets() {
    SequenceStats stats;
    stats.accept(1, 100);
    stats.accept(1, 150);
    stats.accept(2, 0);
    return stats.session == 2 && stats.last == 0 && stats.received == 1 && stats.gaps == 0;
}
constexpr bool wraparound() {
    SequenceStats stats;
    stats.accept(1, 0xfffffffeu);
    stats.accept(1, 0xffffffffu);
    stats.accept(1, 0);
    stats.accept(1, 2);
    return stats.received == 4 && stats.gaps == 1 && stats.last == 2;
}
static_assert(gapsAndDuplicates(), "Gap and duplicate accounting");
static_assert(rebootResets(), "Transmitter reboot resets statistics");
static_assert(wraparound(), "Sequence wraparound accounting");
