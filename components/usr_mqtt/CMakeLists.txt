idf_component_register(SRCS "usr_mqtt.c"
                            "usr_mqtt_msg.c"
                        INCLUDE_DIRS "."
                                    "../usr_common"
                                    "../usr_audio"
                                    "../usr_led"
                        REQUIRES mqtt json esp_peripherals wpa_supplicant esp_http_client
)