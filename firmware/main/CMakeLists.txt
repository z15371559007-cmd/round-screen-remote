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
    INCLUDE_DIRS "." "board" "modules" "platform"
    REQUIRES log esp_common bt esp_hid nvs_flash waveshare__esp32_s3_touch_amoled_1_75 lvgl
)
