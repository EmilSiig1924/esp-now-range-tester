#if defined(RANGE_BUTTON) || defined(RANGE_ECHO)
#include "ping_radio.h"
#include "config.h"
#include "radio.h"
#include <atomic>
#include <cstring>
#include "esp_now.h"
#include "esp_timer.h"
#include "freertos/queue.h"

static QueueHandle_t inbox;
static PingType acceptedType;
static std::atomic<uint32_t> dropped{0};

static void onPing(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    const int64_t arrivedAtUs = esp_timer_get_time();
    if (!info || !info->src_addr || !info->rx_ctrl || !data || len != sizeof(PingPacket)) return;
    PingEvent event{};
    memcpy(&event.packet, data, sizeof(event.packet));
    if (event.packet.magic != pingMagic || event.packet.version != pingVersion ||
        event.packet.type != acceptedType) return;
    memcpy(event.mac, info->src_addr, sizeof(event.mac));
    event.rssi = info->rx_ctrl->rssi;
    event.arrivedAtUs = arrivedAtUs;
    if (xQueueSend(inbox, &event, 0) != pdTRUE) ++dropped;
}

void startPingRadio(const char* role, PingType type) {
    acceptedType = type;
    inbox = xQueueCreate(16, sizeof(PingEvent));
    configASSERT(inbox);
    startRadio(role);
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onPing));
}

bool receivePing(PingEvent& event, TickType_t waitTicks) {
    return xQueueReceive(inbox, &event, waitTicks) == pdTRUE;
}

esp_err_t addPingPeer(const uint8_t* mac) {
    if (esp_now_is_peer_exist(mac)) return ESP_OK;
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac, sizeof(peer.peer_addr));
    peer.channel = config::channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer);
}

uint32_t pingQueueDrops() { return dropped.load(); }
#endif
