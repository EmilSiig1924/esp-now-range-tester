#include "ping.h"

constexpr bool buttonBounceAndHold() {
    ButtonDebouncer button{false, false, 0};
    if (button.update(true, 1000, 30000)) return false;
    if (button.update(false, 2000, 30000)) return false;
    if (button.update(true, 3000, 30000)) return false;
    if (button.update(true, 32999, 30000)) return false;
    if (!button.update(true, 33000, 30000)) return false;
    if (button.update(true, 200000, 30000)) return false; // Holding cannot repeat.
    if (button.update(false, 210000, 30000)) return false;
    if (button.update(false, 240000, 30000)) return false;
    if (button.update(true, 250000, 30000)) return false;
    return button.update(true, 280000, 30000);
}

constexpr bool heldDuringBoot() {
    ButtonDebouncer button{true, true, 0};
    if (button.update(true, 100000, 30000)) return false;
    if (button.update(false, 110000, 30000)) return false;
    if (button.update(false, 140000, 30000)) return false;
    if (button.update(true, 150000, 30000)) return false;
    return button.update(true, 180000, 30000);
}

constexpr bool rejectUnrelatedAndLateReplies() {
    const PendingPing pending{true, 7, 42, 1000};
    PingPacket reply{};
    reply.type = PingType::reply;
    reply.session = 7;
    reply.sequence = 42;
    if (!pending.matches(reply, 2000, 1000000)) return false;
    if (pending.matches(reply, 999, 1000000)) return false;
    if (pending.matches(reply, 1001000, 1000000)) return false;
    reply.sequence = 41;
    if (pending.matches(reply, 2000, 1000000)) return false;
    reply.sequence = 42;
    reply.session = 8;
    if (pending.matches(reply, 2000, 1000000)) return false;
    reply.session = 7;
    reply.type = PingType::request;
    if (pending.matches(reply, 2000, 1000000)) return false;
    reply.type = PingType::reply;
    reply.version = 999;
    if (pending.matches(reply, 2000, 1000000)) return false;
    reply.version = pingVersion;
    reply.magic = 0;
    if (pending.matches(reply, 2000, 1000000)) return false;
    reply.magic = pingMagic;
    return !PendingPing{}.matches(reply, 2000, 1000000);
}

static_assert(buttonBounceAndHold(), "Bounce and held-button suppression");
static_assert(heldDuringBoot(), "Require release after booting with button held");
static_assert(rejectUnrelatedAndLateReplies(), "Reply correlation and timeout boundary");
