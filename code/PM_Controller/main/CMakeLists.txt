idf_component_register(
    SRCS
        "main.c"
        "drivers/st7789/lcd_st7789.c"
        "app/lcd_test/lcd_test.c"
    INCLUDE_DIRS
        "."
        "drivers/st7789"
        "app/lcd_test"
    PRIV_REQUIRES esp_lcd esp_driver_spi esp_driver_gpio freertos heap
)
