#ifdef RANGE_ECHO
#include "ping_radio.h"
#include "ultrasonic.h"
#include <cinttypes>
#include <cstdio>
#include <limits>
#include "esp_now.h"
#include "esp_timer.h"

extern "C" void app_main() {
    startPingRadio("PING RESPONDER", PingType::request);
    startUltrasonic();
    printf("READY | waiting for button requests | no button needed on this board\n");
    fflush(stdout);
    while (true) {
        PingEvent event{};
        if (!receivePing(event, portMAX_DELAY)) continue;
        esp_err_t result = addPingPeer(event.mac);
        if (result != ESP_OK) {
            printf("PEER_ERROR error=%s\n", esp_err_to_name(result));
            fflush(stdout);
            continue;
        }
        PingPacket reply = event.packet;
        reply.type = PingType::reply;
        reply.outwardRssi = event.rssi;
        appendUltrasonicReading(reply); // Snapshot only: never wait for an acoustic echo here.
        const int64_t turnaroundUs = esp_timer_get_time() - event.arrivedAtUs;
        constexpr uint32_t maxTurnaround = std::numeric_limits<uint32_t>::max();
        reply.remoteTurnaroundUs = turnaroundUs > maxTurnaround ? maxTurnaround :
                                  static_cast<uint32_t>(turnaroundUs);
        result = esp_now_send(event.mac, reinterpret_cast<const uint8_t*>(&reply), sizeof(reply));
        // Log only after queuing the reply to avoid serial output inflating RTT.
        printf("ECHO id=%" PRIu32 " outward_rssi=%" PRId32
               " dBm sensor=%s enqueue=%s queue_drops=%" PRIu32 "\n",
               reply.sequence, reply.outwardRssi, sensorStatusName(reply.sensorStatus),
               esp_err_to_name(result), pingQueueDrops());
        fflush(stdout);
    }
}
#endif
