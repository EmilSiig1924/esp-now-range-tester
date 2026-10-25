#pragma once
#include <cstdint>

// Tracks forward sequence progress. Late/duplicate packets are excluded; gaps
// are not retroactively repaired. See README for the measurement limitations.
struct SequenceStats {
    bool started = false;
    uint32_t session = 0;
    uint32_t last = 0;
    uint64_t received = 0;
    uint64_t gaps = 0;
    uint64_t lateOrDuplicate = 0;

    constexpr bool accept(uint32_t newSession, uint32_t sequence) {
        if (!started || newSession != session) {
            *this = SequenceStats{};
            started = true;
            session = newSession;
            last = sequence;
            received = 1;
            return true;
        }
        // Unsigned subtraction also handles sequence wraparound.
        const uint32_t advance = sequence - last;
        if (advance == 0 || advance >= 0x80000000u) {
            ++lateOrDuplicate;
            return false;
        }
        gaps += advance - 1;
        ++received;
        last = sequence;
        return true;
    }

    constexpr double gapPercent() const {
        const uint64_t total = received + gaps;
        return total ? 100.0 * static_cast<double>(gaps) / total : 0.0;
    }
};
