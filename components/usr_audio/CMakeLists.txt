idf_component_register(SRCS "usr_audio.c"
                        INCLUDE_DIRS "."
                                    "../usr_common"
                                    "../usr_mqtt"
                                    "../usr_led"
                        REQUIRES mqtt json esp_peripherals audio_stream input_key_service
)