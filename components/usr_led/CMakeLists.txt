idf_component_register(SRCS "usr_led.c"
                        INCLUDE_DIRS "." 
                                    "../usr_common"
                                    "../usr_mqtt"         
                        REQUIRES nvs_flash esp_peripherals mqtt json wpa_supplicant esp_http_client audio_board
)