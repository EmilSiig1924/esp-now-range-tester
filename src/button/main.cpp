#ifdef RANGE_BUTTON
#include "config.h"
#include "lamp.h"
#include "ping_radio.h"
#include <cinttypes>
#include <cstdio>
#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"

extern "C" void app_main() {
    static_assert(config::buttonGpio >= 0 && config::buttonGpio < 49, "Invalid S3 GPIO");
    const auto buttonPin = static_cast<gpio_num_t>(config::buttonGpio);
    gpio_config_t button{};
    button.pin_bit_mask = 1ULL << config::buttonGpio;
    button.mode = GPIO_MODE_INPUT;
    button.pull_up_en = GPIO_PULLUP_ENABLE;
    button.pull_down_en = GPIO_PULLDOWN_DISABLE;
    button.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&button));

    static_assert(config::lampGpio >= 0 && config::lampGpio < 49 &&
                  config::lampGpio != config::buttonGpio, "Invalid S3 lamp GPIO");
    const auto lampPin = static_cast<gpio_num_t>(config::lampGpio);
    const auto lampLevel = [](bool on) { return on == config::lampActiveHigh ? 1 : 0; };
    // Latch the OFF level before enabling the output so the lamp cannot blip at boot.
    ESP_ERROR_CHECK(gpio_set_level(lampPin, lampLevel(false)));
    gpio_config_t lampOutput{};
    lampOutput.pin_bit_mask = 1ULL << config::lampGpio;
    lampOutput.mode = GPIO_MODE_OUTPUT;
    lampOutput.pull_up_en = GPIO_PULLUP_DISABLE;
    lampOutput.pull_down_en = GPIO_PULLDOWN_DISABLE;
    lampOutput.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&lampOutput));
    ESP_ERROR_CHECK(gpio_set_level(lampPin, lampLevel(false)));

    startPingRadio("BUTTON INITIATOR", PingType::reply);
    const uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    ESP_ERROR_CHECK(addPingPeer(broadcast));
    const uint32_t session = esp_random();
    uint32_t sequence = 0;
    uint32_t attempts = 0, replies = 0, timeouts = 0, localErrors = 0;
    const bool initiallyPressed = gpio_get_level(buttonPin) == 0;
    ButtonDebouncer debouncer{initiallyPressed, initiallyPressed, esp_timer_get_time()};
    PendingPing pending;
    // Steady while a request is outstanding; then one long flash for a reply,
    // short flashes for a timeout, and rapid flashes for a local send error.
    LampBlinker blinker;
    bool lampOn = false;
    const auto updateLamp = [&](int64_t nowUs) {
        const bool on = pending.active || blinker.lit(nowUs);
        if (on == lampOn) return;
        lampOn = on;
        gpio_set_level(lampPin, lampLevel(on));
    };
    constexpr int64_t timeoutUs = config::pingTimeoutMs * 1000LL;
    TickType_t pollTicks = pdMS_TO_TICKS(10);
    if (pollTicks == 0) pollTicks = 1;
    printf("READY | button=GPIO%d | COM=GND NO=GPIO%d | lamp=GPIO%d | release then press | timeout_ms=%" PRIu32 "\n",
           config::buttonGpio, config::buttonGpio, config::lampGpio, config::pingTimeoutMs);
    fflush(stdout);

    while (true) {
        PingEvent event{};
        if (receivePing(event, pollTicks) &&
            pending.matches(event.packet, event.arrivedAtUs, timeoutUs)) {
            const int64_t rttUs = event.arrivedAtUs - pending.startedAtUs;
            pending.active = false; // Reject duplicate/late replies for this request.
            ++replies;
            blinker.start(1, config::lampReplyFlashMs * 1000LL, 0, event.arrivedAtUs);
            printf("PONG id=%" PRIu32 " rtt_ms=%.3f outward_rssi=%" PRId32
                   " dBm return_rssi=%" PRId32 " dBm remote_turnaround_us=%" PRIu32
                   " sensor=%s",
                   pending.sequence, rttUs / 1000.0, event.packet.outwardRssi, event.rssi,
                   event.packet.remoteTurnaroundUs, sensorStatusName(event.packet.sensorStatus));
            if (event.packet.sensorStatus == SensorStatus::ok) {
                printf(" distance_mm=%" PRIu32, event.packet.distanceMm);
            } else {
                printf(" distance_mm=NA");
            }
            if (event.packet.sensorAgeMs != unknownSensorAge) {
                printf(" sensor_age_ms=%" PRIu32, event.packet.sensorAgeMs);
            } else {
                printf(" sensor_age_ms=NA");
            }
            printf(" attempts=%" PRIu32 " replies=%" PRIu32 " timeouts=%" PRIu32
                   " local_errors=%" PRIu32 " queue_drops=%" PRIu32
                   " peer=%02X:%02X:%02X:%02X:%02X:%02X\n",
                   attempts, replies, timeouts, localErrors,
                   pingQueueDrops(), event.mac[0], event.mac[1], event.mac[2],
                   event.mac[3], event.mac[4], event.mac[5]);
            fflush(stdout);
        }

        const int64_t nowUs = esp_timer_get_time();
        if (pending.active && nowUs - pending.startedAtUs >= timeoutUs) {
            pending.active = false;
            ++timeouts;
            blinker.start(config::lampTimeoutFlashes, config::lampTimeoutFlashMs * 1000LL,
                          config::lampTimeoutFlashMs * 1000LL, nowUs);
            printf("TIMEOUT id=%" PRIu32 " after_ms=%" PRIu32
                   " attempts=%" PRIu32 " replies=%" PRIu32 " timeouts=%" PRIu32
                   " local_errors=%" PRIu32 " queue_drops=%" PRIu32 "\n",
                   pending.sequence, config::pingTimeoutMs, attempts, replies, timeouts,
                   localErrors, pingQueueDrops());
            fflush(stdout);
        }
        updateLamp(nowUs);
        if (!debouncer.update(gpio_get_level(buttonPin) == 0, nowUs,
                              config::buttonDebounceMs * 1000LL)) continue;
        if (pending.active) {
            printf("BUSY | wait for reply or timeout, then release and press again\n");
            fflush(stdout);
            continue;
        }

        PingPacket request{};
        request.session = session;
        request.sequence = ++sequence;
        ++attempts;
        pending = PendingPing{true, session, sequence, esp_timer_get_time()};
        const esp_err_t result = esp_now_send(broadcast,
            reinterpret_cast<const uint8_t*>(&request), sizeof(request));
        if (result != ESP_OK) {
            pending.active = false;
            ++localErrors;
            blinker.start(config::lampErrorFlashes, config::lampErrorFlashMs * 1000LL,
                          config::lampErrorFlashMs * 1000LL, esp_timer_get_time());
            printf("SEND_ERROR id=%" PRIu32 " error=%s local_errors=%" PRIu32 "\n",
                   sequence, esp_err_to_name(result), localErrors);
            fflush(stdout);
        }
        updateLamp(esp_timer_get_time()); // Light at once rather than on the next poll.
    }
}
#endif
