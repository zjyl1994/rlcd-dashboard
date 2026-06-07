idf_component_register(
    SRCS "display_bsp.cpp" "i2c_bsp.cpp" "i2c_equipment.cpp" "adc_bsp.cpp"
    PRIV_REQUIRES driver esp_lcd esp_timer esp_adc
    REQUIRES SensorLib
    INCLUDE_DIRS "./")
