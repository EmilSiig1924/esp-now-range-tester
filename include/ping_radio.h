#pragma once
#include "ping.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

struct PingEvent {
    PingPacket packet;
    uint8_t mac[6];
    int32_t rssi;
    int64_t arrivedAtUs;
};

void startPingRadio(const char* role, PingType acceptedType);
bool receivePing(PingEvent& event, TickType_t waitTicks);
esp_err_t addPingPeer(const uint8_t* mac);
uint32_t pingQueueDrops();
