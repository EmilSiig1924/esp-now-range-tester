#pragma once
#include <cstdint>

namespace config {
constexpr uint8_t channel = 1;           // Must match on both boards.
constexpr uint32_t sendIntervalMs = 100; // 10 packets per second.
constexpr uint32_t reportIntervalMs = 1000;
constexpr uint32_t staleAfterMs = 2000;
constexpr uint32_t magic = 0x524E4731;    // Identifies this project's packets.
constexpr uint32_t version = 1;
constexpr int buttonGpio = 4;            // S3: isolated COM -> GND, NO -> GPIO4.
constexpr uint32_t buttonDebounceMs = 30;
constexpr uint32_t pingTimeoutMs = 1000;
constexpr uint32_t pingIntervalMs = 500;  // Press toggles pinging; one request per interval.
constexpr int lampGpio = 5;              // S3: 2 mA lamp via 100 Ohm; higher-current lamps need a transistor.
constexpr bool lampActiveHigh = true;    // HIGH turns the lamp on.
constexpr uint32_t lampReplyFlashMs = 150; // Shorter than pingIntervalMs: a steady blink while running.
constexpr uint32_t lampTimeoutFlashes = 3;
constexpr uint32_t lampTimeoutFlashMs = 100;
constexpr uint32_t lampErrorFlashes = 6;
constexpr uint32_t lampErrorFlashMs = 50;
constexpr int displaySdaGpio = 8;        // S3: 0.91" SSD1306 OLED, powered from 3V3.
constexpr int displaySclGpio = 9;        // Module pin labelled SCK.
constexpr bool displayFlip = false;      // Set true if the text appears upside down.
constexpr int ultrasonicTrigGpio = 18;   // C6, AJ-SR04M mode 1 (R19 open).
constexpr int ultrasonicEchoGpio = 19;   // Input must stay within 3.3 V logic levels.
constexpr uint32_t ultrasonicIntervalMs = 250;
constexpr uint32_t ultrasonicStaleAfterMs = 1000;
}
