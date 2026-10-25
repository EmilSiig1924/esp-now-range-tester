#if !defined(RANGE_TRANSMITTER) && !defined(RANGE_BUTTON) && !defined(RANGE_ECHO)
#include "config.h"
#include "packet.h"
#include "radio.h"
#include "statistics.h"
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include "esp_now.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct ReceivedPacket {
    TestPacket packet;
    uint8_t mac[6];
    int8_t rssi;
    int64_t receivedAtUs;
};
static QueueHandle_t receivedQueue;
static std::atomic<uint32_t> queueDrops{0};

// This runs on the Wi-Fi task. Copy data into a queue; do the printing and
// statistics later in app_main so the callback never blocks the radio.
static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!info || !info->rx_ctrl || !data || len != sizeof(TestPacket)) return;
    ReceivedPacket event{};
    memcpy(&event.packet, data, sizeof(event.packet));
    if (event.packet.magic != config::magic || event.packet.version != config::version) return;
    memcpy(event.mac, info->src_addr, sizeof(event.mac));
    event.rssi = info->rx_ctrl->rssi;
    event.receivedAtUs = esp_timer_get_time();
    if (xQueueSend(receivedQueue, &event, 0) != pdTRUE) ++queueDrops;
}

extern "C" void app_main() {
    receivedQueue = xQueueCreate(64, sizeof(ReceivedPacket));
    configASSERT(receivedQueue);
    startRadio("RECEIVER");
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onReceive));
    SequenceStats stats;
    uint8_t selectedMac[6]{};
    bool selected = false;
    int rssi = 0;
    uint32_t txUptime = 0;
    int64_t lastPacketUs = 0;
    int64_t lastReportUs = esp_timer_get_time();

    while (true) {
        ReceivedPacket event{};
        if (xQueueReceive(receivedQueue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!selected) {
                memcpy(selectedMac, event.mac, sizeof(selectedMac));
                selected = true;
                printf("Tracking transmitter %02X:%02X:%02X:%02X:%02X:%02X\n",
                       selectedMac[0], selectedMac[1], selectedMac[2],
                       selectedMac[3], selectedMac[4], selectedMac[5]);
            }
            if (memcmp(selectedMac, event.mac, sizeof(selectedMac)) == 0) {
                if (stats.accept(event.packet.session, event.packet.sequence)) {
                    txUptime = event.packet.uptimeMs;
                }
                rssi = event.rssi;
                lastPacketUs = event.receivedAtUs;
            }
        }
        const int64_t now = esp_timer_get_time();
        if (now - lastReportUs < config::reportIntervalMs * 1000LL) continue;
        lastReportUs = now;
        if (!stats.started) {
            printf("WAITING | channel=%u | no test packets received\n", config::channel);
        } else {
            const int64_t ageMs = (now - lastPacketUs) / 1000;
            printf("%s session=%08" PRIX32 " seq=%" PRIu32
                   " rssi=%d dBm rx=%" PRIu64 " gaps=%" PRIu64
                   " gap_pct=%.1f late_dup=%" PRIu64 " age_ms=%" PRId64
                   " tx_uptime_ms=%" PRIu32 " queue_drops=%" PRIu32 "\n",
                   ageMs >= config::staleAfterMs ? "STALE" : "LIVE",
                   stats.session, stats.last, rssi, stats.received, stats.gaps,
                   stats.gapPercent(), stats.lateOrDuplicate, ageMs,
                   txUptime, queueDrops.load());
        }
        fflush(stdout);
    }
}
#endif
