#ifndef __USR_MQTT_MSG_H__
#define __USR_MQTT_MSG_H__
#include "usr_common.h"




esp_err_t speaker_stop_talk(void);

esp_err_t get_upload_music_url_json(char *json, char *url, char *text);

esp_err_t mqtt_subscribe_msg_precess(char *topic, int topic_len, char*data, int data_len);

esp_err_t speaker_start_talk(char *filename, char *text);

esp_err_t speaker_talk_role_init(void);

esp_err_t mqtt_username_passwd_get(char *json, char *mqtt_username, char *mqtt_passwd);

int audio_control_handle(char *json, char *url, uint32_t *sessionId, uint32_t *audioNum);

esp_err_t mode_audio_init(char *json);
#endif /* __USR_MQTT_MSG_H__ */
