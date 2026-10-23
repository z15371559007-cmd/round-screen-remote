# Backend selection (see Kconfig "Host link" / "Pet voice backend").
if(CONFIG_ROUND_REMOTE_HOST_LINK_BACKEND_WIFI_WS)
    message(FATAL_ERROR "ROUND_REMOTE_HOST_LINK_BACKEND_WIFI_WS is not implemented yet; select the stub")
endif()
if(CONFIG_ROUND_REMOTE_PET_VOICE_BACKEND_ESP_SR)
    message(FATAL_ERROR "ROUND_REMOTE_PET_VOICE_BACKEND_ESP_SR is not implemented yet; select the stub")
endif()

idf_component_register(
    SRCS
        "app_main.c"
        "mode_router.c"
        "board/board_port_waveshare.c"
        "modules/media_remote.c"
        "modules/push_to_talk.c"
        "modules/pet_standby.c"
        "modules/pet_commands.c"
        "platform/media_link.c"
        "platform/ble_hid.c"
        "platform/esp_hid_gap.c"
        "platform/pet_voice_backend_stub.c"
        "platform/host_link_stub.c"
        "platform/host_link_protocol.c"
        "platform/pmu_axp2101.c"
        "platform/hw_buttons.c"
        "modules/button_actions.c"
        "modules/badge.c"
        "fonts/rsr_font_cjk_24.c"
    EMBED_TXTFILES "web/upload.html"
    INCLUDE_DIRS "." "board" "modules" "platform" "fonts"
    REQUIRES log esp_common esp_timer esp_driver_gpio esp_driver_i2c bt esp_hid nvs_flash esp_wifi esp_netif esp_event esp_http_server esp_partition spi_flash esp_psram waveshare__esp32_s3_touch_amoled_1_75 lvgl
)
