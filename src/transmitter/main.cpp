#ifdef RANGE_TRANSMITTER
#include "config.h"
#include "packet.h"
#include "radio.h"
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main() {
    startRadio("TRANSMITTER");
    const uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, broadcast, sizeof(broadcast));
    peer.channel = config::channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    TestPacket packet{config::magic, config::version, esp_random(), 0, 0};
    uint32_t enqueueErrors = 0;
    int64_t lastReport = 0;
    TickType_t nextWake = xTaskGetTickCount();
    while (true) {
        packet.uptimeMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        const esp_err_t result = esp_now_send(broadcast,
            reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
        if (result != ESP_OK) ++enqueueErrors;

        const int64_t now = esp_timer_get_time();
        if (now - lastReport >= config::reportIntervalMs * 1000LL) {
            printf("TX session=%08" PRIX32 " seq=%" PRIu32
                   " uptime_ms=%" PRIu32 " enqueue_errors=%" PRIu32 "\n",
                   packet.session, packet.sequence, packet.uptimeMs, enqueueErrors);
            fflush(stdout);
            lastReport = now;
        }
        // Sequence advances even on a local send error; receiver gaps therefore
        // describe attempted transmissions, not exclusively over-the-air loss.
        ++packet.sequence;
        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(config::sendIntervalMs));
    }
}
#endif
