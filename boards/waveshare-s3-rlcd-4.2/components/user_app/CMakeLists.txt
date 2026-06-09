idf_component_register(
    SRCS "user_app.cpp"
    REQUIRES port_bsp ui_bsp app_bsp main
    PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_timer esp_netif esp_http_server mqtt driver json codec_board
    EMBED_FILES "beep1.pcm" "beep2.pcm" "beep4.pcm" "beep5.pcm" "beep6.pcm"
    INCLUDE_DIRS "./")
