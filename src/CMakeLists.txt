idf_component_register(
    SRCS "common/radio.cpp" "common/ping_radio.cpp"
         "transmitter/main.cpp" "receiver/main.cpp"
         "button/main.cpp" "echo/main.cpp" "echo/ultrasonic.cpp"
    INCLUDE_DIRS "../include"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash esp_driver_gpio esp_driver_rmt
)
