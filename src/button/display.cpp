#ifdef RANGE_BUTTON
#include "display.h"
#include "config.h"
#include <cstdio>
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace {
constexpr uint8_t candidateAddresses[] = {0x3C, 0x3D};
QueueHandle_t pendingScreen;
esp_lcd_panel_handle_t panel;

// I2C transfers take about 15 ms per frame. Drawing in this low-priority task
// keeps them away from button polling, the lamp, and the reply timestamps.
void displayTask(void*) {
    TextFrame frame;
    ScreenText screen;
    while (true) {
        if (xQueueReceive(pendingScreen, &screen, portMAX_DELAY) != pdTRUE) continue;
        frame.clear();
        for (int line = 0; line < TextFrame::lines; ++line) frame.drawText(line, screen.line[line]);
        const esp_err_t result = esp_lcd_panel_draw_bitmap(panel, 0, 0, TextFrame::width,
                                                           TextFrame::lines * 8, frame.pixels);
        if (result != ESP_OK) {
            printf("DISPLAY_ERROR error=%s\n", esp_err_to_name(result));
            fflush(stdout);
        }
    }
}

esp_err_t createPanel(i2c_master_bus_handle_t bus, uint8_t address) {
    esp_lcd_panel_io_i2c_config_t ioConfig{};
    ioConfig.dev_addr = address;
    ioConfig.scl_speed_hz = 400000;
    ioConfig.control_phase_bytes = 1; // SSD1306 datasheet values.
    ioConfig.dc_bit_offset = 6;
    ioConfig.lcd_cmd_bits = 8;
    ioConfig.lcd_param_bits = 8;
    esp_lcd_panel_io_handle_t io = nullptr;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &ioConfig, &io), "display", "panel io");

    esp_lcd_panel_ssd1306_config_t ssd1306{};
    ssd1306.height = TextFrame::lines * 8;
    esp_lcd_panel_dev_config_t panelConfig{};
    panelConfig.bits_per_pixel = 1;
    panelConfig.reset_gpio_num = -1; // Four-pin modules have no reset line.
    panelConfig.vendor_config = &ssd1306;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ssd1306(io, &panelConfig, &panel), "display", "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), "display", "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), "display", "init");
    // Both mirrors on gives upright text on common 0.91" modules; flip rotates 180 degrees.
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel, !config::displayFlip, !config::displayFlip),
                        "display", "mirror");
    const TextFrame blank;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, 0, TextFrame::width,
                                                  TextFrame::lines * 8, blank.pixels),
                        "display", "clear");
    return esp_lcd_panel_disp_on_off(panel, true);
}
} // namespace

bool startDisplay() {
    static_assert(config::displaySdaGpio != config::displaySclGpio, "Use separate I2C pins");
    i2c_master_bus_config_t busConfig{};
    busConfig.i2c_port = I2C_NUM_0;
    busConfig.sda_io_num = static_cast<gpio_num_t>(config::displaySdaGpio);
    busConfig.scl_io_num = static_cast<gpio_num_t>(config::displaySclGpio);
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = true; // Modules usually add their own as well.
    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t result = i2c_new_master_bus(&busConfig, &bus);

    int address = -1;
    for (const uint8_t candidate : candidateAddresses) {
        if (result == ESP_OK && i2c_master_probe(bus, candidate, 50) == ESP_OK) {
            address = candidate;
            break;
        }
    }
    if (result == ESP_OK && address < 0) result = ESP_ERR_NOT_FOUND;
    if (result == ESP_OK) result = createPanel(bus, static_cast<uint8_t>(address));
    if (result == ESP_OK) {
        pendingScreen = xQueueCreate(1, sizeof(ScreenText));
        result = pendingScreen ? ESP_OK : ESP_ERR_NO_MEM;
    }
    if (result == ESP_OK && xTaskCreate(displayTask, "display", 3072, nullptr, 1, nullptr) != pdPASS) {
        result = ESP_ERR_NO_MEM;
    }
    if (result != ESP_OK) {
        pendingScreen = nullptr;
        printf("DISPLAY none | SDA=GPIO%d SCL=GPIO%d | error=%s | continuing without it\n",
               config::displaySdaGpio, config::displaySclGpio, esp_err_to_name(result));
    } else {
        printf("DISPLAY SSD1306 128x32 | SDA=GPIO%d SCL=GPIO%d | address=0x%02X\n",
               config::displaySdaGpio, config::displaySclGpio, address);
    }
    fflush(stdout);
    return result == ESP_OK;
}

void showScreen(const ScreenText& screen) {
    if (pendingScreen) xQueueOverwrite(pendingScreen, &screen);
}
#endif
