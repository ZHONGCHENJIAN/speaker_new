idf_component_register(SRCS "usr_common.c"
                        INCLUDE_DIRS "."
                                    "../usr_mqtt"
                                    "../usr_audio"
                        REQUIRES nvs_flash esp_peripherals
)