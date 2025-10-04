idf_component_register(SRCS "main.c"
                            "zigbee_core.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash driver spi_flash esp_common esp_event esp-zigbee-lib esp-zboss-lib)