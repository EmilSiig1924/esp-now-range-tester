#ifdef RANGE_ECHO
#include "ultrasonic.h"
#include "config.h"
#include "ping.h"
#include <cinttypes>
#include <cstdio>
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"

namespace {
constexpr uint32_t resolutionHz = 500000; // 2 us ticks; supports > 8 m echo times.
constexpr uint32_t idleNs = 60000000;     // 60 ms; below the 15-bit hardware limit.
constexpr uint32_t receiveTimeoutMs = 180;
static_assert(config::ultrasonicIntervalMs >= 200, "Leave time between acoustic measurements");
static_assert(config::ultrasonicTrigGpio != config::ultrasonicEchoGpio, "Use separate sensor pins");
static_assert(GPIO_IS_VALID_OUTPUT_GPIO(config::ultrasonicTrigGpio), "Trigger must be an output-capable GPIO");
static_assert(GPIO_IS_VALID_GPIO(config::ultrasonicEchoGpio), "Echo must be a valid GPIO");

struct Sample {
    DistanceReading reading;
    int64_t triggeredAtUs = 0;
};
QueueHandle_t latestSample;
QueueHandle_t completedCapture;
rmt_channel_handle_t rxChannel;
rmt_symbol_word_t symbols[64]; // Static: remains alive until RMT completes.

struct EchoEdges {
    uint32_t rises = 0;
    uint32_t falls = 0;
    uint32_t pulseUs = 0;
    int64_t roseAtUs = -1;
};
EchoEdges echoEdges;
portMUX_TYPE echoEdgeMux = portMUX_INITIALIZER_UNLOCKED;

void onEchoEdge(void*) {
    const int level = gpio_get_level(static_cast<gpio_num_t>(config::ultrasonicEchoGpio));
    const int64_t nowUs = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&echoEdgeMux);
    if (level) {
        ++echoEdges.rises;
        echoEdges.roseAtUs = nowUs;
    } else {
        ++echoEdges.falls;
        if (echoEdges.roseAtUs >= 0) {
            const int64_t widthUs = nowUs - echoEdges.roseAtUs;
            echoEdges.pulseUs = widthUs <= 0xffffffffLL ? static_cast<uint32_t>(widthUs) : 0;
        }
        echoEdges.roseAtUs = -1;
    }
    portEXIT_CRITICAL_ISR(&echoEdgeMux);
}

EchoEdges edgeSnapshot() {
    portENTER_CRITICAL(&echoEdgeMux);
    const EchoEdges snapshot = echoEdges;
    portEXIT_CRITICAL(&echoEdgeMux);
    return snapshot;
}

TickType_t ticksForMs(uint32_t ms) {
    const TickType_t ticks = pdMS_TO_TICKS(ms);
    return ticks ? ticks : 1;
}

bool onCapture(rmt_channel_handle_t, const rmt_rx_done_event_data_t* data, void*) {
    BaseType_t woken = pdFALSE;
    // Copy the event descriptor; its symbol buffer is the static array above.
    xQueueSendFromISR(completedCapture, data, &woken);
    return woken == pdTRUE;
}

DistanceReading decodeCapture(const rmt_rx_done_event_data_t& capture, uint32_t& highPulseUs) {
    // A clean mode-1 echo has exactly one contiguous HIGH pulse. Do not turn
    // multiple noise pulses into a plausible distance by summing them.
    uint32_t highTicks = 0;
    bool highEnded = false;
    for (size_t i = 0; i < capture.num_symbols; ++i) {
        const auto& symbol = capture.received_symbols[i];
        const uint32_t durations[2] = {symbol.duration0, symbol.duration1};
        const uint32_t levels[2] = {symbol.level0, symbol.level1};
        for (int half = 0; half < 2; ++half) {
            if (durations[half] == 0) return distanceFromPulseUs(highPulseUs);
            if (levels[half]) {
                if (highEnded) return {SensorStatus::error, 0};
                highTicks += durations[half];
                highPulseUs = highTicks * 2;
            } else if (highTicks) {
                highEnded = true;
            }
        }
    }
    // A full buffer could have clipped the echo. Never publish a partial pulse.
    if (capture.num_symbols >= sizeof(symbols) / sizeof(symbols[0])) {
        return {SensorStatus::error, 0};
    }
    return distanceFromPulseUs(highPulseUs);
}

void sensorTask(void*) {
    const auto triggerPin = static_cast<gpio_num_t>(config::ultrasonicTrigGpio);
    rmt_receive_config_t receiveConfig{};
    receiveConfig.signal_range_min_ns = 1000;
    receiveConfig.signal_range_max_ns = idleNs;
    TickType_t nextWake = xTaskGetTickCount();
    int64_t lastLogUs = 0;
    uint32_t triggerAttempts = 0;
    int triggerHigh = -1, triggerLow = -1;
    uint32_t triggerLatch = 0, triggerOutputEnabled = 0;
    bool triggerHeld = false;
    bool triggerHealthy = true;
    while (true) {
        xQueueReset(completedCapture);
        Sample sample{};
        sample.triggeredAtUs = esp_timer_get_time();
        const char* reason = "capture";
        esp_err_t captureError = ESP_OK;
        uint32_t highPulseUs = 0;
        rmt_rx_done_event_data_t capture{};
        bool gotCapture = false;
        const EchoEdges edgesBefore = edgeSnapshot();
        const int echoBefore = gpio_get_level(static_cast<gpio_num_t>(config::ultrasonicEchoGpio));
        // The external divider's lower resistor holds this input LOW when the
        // sensor output is disconnected. A HIGH before triggering is invalid.
        if (!triggerHealthy) {
            sample.reading = {SensorStatus::error, 0};
            reason = "trigger_output_fault_reset_required";
        } else if (echoBefore) {
            sample.reading = {SensorStatus::error, 0};
            reason = "echo_high_before_trigger";
        } else {
            const esp_err_t result = rmt_receive(rxChannel, symbols, sizeof(symbols), &receiveConfig);
            if (result != ESP_OK) {
                sample.reading = {SensorStatus::error, 0};
                reason = "capture_arm_failed";
                captureError = result;
            } else {
                ++triggerAttempts;
                ESP_ERROR_CHECK(gpio_set_level(triggerPin, 1));
                esp_rom_delay_us(2); // Allow pad input feedback to settle.
                triggerHigh = gpio_get_level(triggerPin);
                triggerLatch = (GPIO.out.val >> config::ultrasonicTrigGpio) & 1U;
                triggerOutputEnabled = (GPIO.enable.val >> config::ultrasonicTrigGpio) & 1U;
                triggerHeld = gpio_ll_is_digital_io_hold(&GPIO, config::ultrasonicTrigGpio);
                esp_rom_delay_us(18); // About 20 us HIGH, plus function-call overhead.
                ESP_ERROR_CHECK(gpio_set_level(triggerPin, 0));
                esp_rom_delay_us(2);
                triggerLow = gpio_get_level(triggerPin);
                triggerHealthy = triggerHigh == 1 && triggerLow == 0;
                if (!triggerHealthy) {
                    ESP_ERROR_CHECK(rmt_disable(rxChannel));
                    ESP_ERROR_CHECK(rmt_enable(rxChannel));
                    sample.reading = {SensorStatus::error, 0};
                    reason = "trigger_output_fault_reset_required";
                } else if (xQueueReceive(completedCapture, &capture, ticksForMs(receiveTimeoutMs)) == pdTRUE) {
                    gotCapture = true;
                    sample.reading = decodeCapture(capture, highPulseUs);
                    if (sample.reading.status == SensorStatus::noEcho) reason = "no_high_pulse";
                } else {
                    // Cancel any pending capture before reusing its buffer next cycle.
                    ESP_ERROR_CHECK(rmt_disable(rxChannel));
                    ESP_ERROR_CHECK(rmt_enable(rxChannel));
                    sample.reading = {SensorStatus::noEcho, 0};
                    reason = "capture_timeout";
                }
            }
        }
        // Replace even a previously good reading on failure; never silently
        // present an old valid distance after the sensor stops responding.
        xQueueOverwrite(latestSample, &sample);
        const EchoEdges edgesAfter = edgeSnapshot();
        const int64_t nowUs = esp_timer_get_time();
        if (nowUs - lastLogUs >= 1000000) {
            printf("SENSOR status=%s distance_mm=", sensorStatusName(sample.reading.status));
            if (sample.reading.status == SensorStatus::ok) {
                printf("%" PRIu32, sample.reading.distanceMm);
            } else {
                printf("NA");
            }
            printf(" reason=%s pulse_us=%" PRIu32 " symbols=%u echo_before=%d echo_now=%d rmt=%s",
                   reason, highPulseUs, gotCapture ? static_cast<unsigned>(capture.num_symbols) : 0,
                   echoBefore, gpio_get_level(static_cast<gpio_num_t>(config::ultrasonicEchoGpio)),
                   esp_err_to_name(captureError));
            printf(" gpio_rises=%" PRIu32 " gpio_falls=%" PRIu32 " gpio_pulse_us=%" PRIu32,
                   edgesAfter.rises - edgesBefore.rises, edgesAfter.falls - edgesBefore.falls,
                   edgesAfter.falls != edgesBefore.falls ? edgesAfter.pulseUs : 0);
            printf(" trig_gpio=%d trig_attempts=%" PRIu32 " trig_read=%d/%d",
                   config::ultrasonicTrigGpio, triggerAttempts, triggerHigh, triggerLow);
            printf(" trig_latch=%" PRIu32 " trig_oe=%" PRIu32 " trig_hold=%d",
                   triggerLatch, triggerOutputEnabled, triggerHeld);
            if (gotCapture) {
                printf(" raw_us=");
                for (size_t i = 0; i < capture.num_symbols && i < 3; ++i) {
                    const auto& symbol = capture.received_symbols[i];
                    printf("%s%u:%u/%u:%u", i ? "," : "",
                           static_cast<unsigned>(symbol.level0), static_cast<unsigned>(symbol.duration0) * 2,
                           static_cast<unsigned>(symbol.level1), static_cast<unsigned>(symbol.duration1) * 2);
                }
            }
            printf("\n");
            if (!triggerHealthy) {
                ESP_ERROR_CHECK(gpio_dump_io_configuration(stdout,
                    (1ULL << config::ultrasonicTrigGpio) | (1ULL << config::ultrasonicEchoGpio)));
            }
            fflush(stdout);
            lastLogUs = nowUs;
        }
        vTaskDelayUntil(&nextWake, ticksForMs(config::ultrasonicIntervalMs));
    }
}
} // namespace

void startUltrasonic() {
    latestSample = xQueueCreate(1, sizeof(Sample));
    completedCapture = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    configASSERT(latestSample && completedCapture);
    const Sample initial{};
    xQueueOverwrite(latestSample, &initial);

    gpio_config_t trigger{};
    trigger.pin_bit_mask = 1ULL << config::ultrasonicTrigGpio;
    trigger.mode = GPIO_MODE_INPUT_OUTPUT; // Read actual pad state as well as driving it.
    ESP_ERROR_CHECK(gpio_config(&trigger));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(config::ultrasonicTrigGpio), 0));

    rmt_rx_channel_config_t rx{};
    rx.gpio_num = static_cast<gpio_num_t>(config::ultrasonicEchoGpio);
    rx.clk_src = RMT_CLK_SRC_DEFAULT;
    rx.resolution_hz = resolutionHz;
    rx.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx, &rxChannel));
    // Internal pulls would load the external divider and reduce its HIGH level.
    ESP_ERROR_CHECK(gpio_set_pull_mode(rx.gpio_num, GPIO_FLOATING));
    // Independent diagnostic only: distance still comes from hardware RMT.
    const esp_err_t isrService = gpio_install_isr_service(0);
    if (isrService != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(isrService);
    ESP_ERROR_CHECK(gpio_set_intr_type(rx.gpio_num, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(rx.gpio_num, onEchoEdge, nullptr));
    rmt_rx_event_callbacks_t callbacks{};
    callbacks.on_recv_done = onCapture;
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rxChannel, &callbacks, nullptr));
    ESP_ERROR_CHECK(rmt_enable(rxChannel));
    ESP_ERROR_CHECK(gpio_dump_io_configuration(stdout,
        (1ULL << config::ultrasonicTrigGpio) | (1ULL << config::ultrasonicEchoGpio)));
    // Task creation must run even in builds where assertion checks are disabled.
    const BaseType_t taskResult = xTaskCreate(sensorTask, "ultrasonic", 4096, nullptr, 3, nullptr);
    ESP_ERROR_CHECK(taskResult == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    printf("AJ-SR04M mode=1 diag=trigger-readback-1 | TRIG=GPIO%d ECHO=GPIO%d | interval_ms=%" PRIu32 "\n",
           config::ultrasonicTrigGpio, config::ultrasonicEchoGpio, config::ultrasonicIntervalMs);
}

void appendUltrasonicReading(PingPacket& packet) {
    Sample sample{};
    if (xQueuePeek(latestSample, &sample, 0) != pdTRUE) return;
    packet.sensorStatus = sample.reading.status;
    packet.distanceMm = 0;
    packet.sensorAgeMs = unknownSensorAge;
    if (sample.reading.status == SensorStatus::notReady) return;
    const int64_t ageMs = (esp_timer_get_time() - sample.triggeredAtUs) / 1000;
    packet.sensorAgeMs = ageMs >= unknownSensorAge ? unknownSensorAge : static_cast<uint32_t>(ageMs);
    packet.sensorStatus = sensorStatusForAge(sample.reading.status, packet.sensorAgeMs,
                                           config::ultrasonicStaleAfterMs);
    if (packet.sensorStatus == SensorStatus::ok) packet.distanceMm = sample.reading.distanceMm;
}
#endif
