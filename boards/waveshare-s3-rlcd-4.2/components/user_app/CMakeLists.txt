idf_component_register(
    SRCS "user_app.cpp"
    REQUIRES port_bsp ui_bsp app_bsp main
    PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_timer esp_netif esp_http_server mqtt driver json codec_board
    INCLUDE_DIRS "./")
