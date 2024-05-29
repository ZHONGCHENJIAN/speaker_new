#ifndef __USR_AUDIO_H__
#define __USR_AUDIO_H__
#include "esp_peripherals.h"

#define MAX_MODE_ID                    (10)

void play_audio_set_url(char *url);
void usr_audio_start();
void usr_wifi_start();
extern esp_periph_config_t periph_cfg;
extern esp_periph_set_handle_t set;
extern QueueHandle_t audio_play_queue;
extern char *speaker_mode_audio_url[10];
extern char *speaker_mode_audio_id[10];
#endif /* __USR_AUDIO_H__ */