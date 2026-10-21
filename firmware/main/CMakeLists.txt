idf_component_register(
    SRCS
        "app_main.c"
        "mode_router.c"
        "board/board_port_stub.c"
        "modules/media_remote.c"
        "modules/push_to_talk.c"
        "modules/pet_standby.c"
        "modules/pet_commands.c"
        "platform/media_link_stub.c"
        "platform/ble_hid_stub.c"
        "platform/pet_voice_backend_stub.c"
    INCLUDE_DIRS "." "board" "modules" "platform"
    REQUIRES log esp_common
)
