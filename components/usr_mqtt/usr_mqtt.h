#ifndef __USR_MQTT_H__
#define __USR_MQTT_H__
#include "usr_common.h"


typedef enum {
    TOPIC_EVENT = 0,
    TOPIC_CONTROL,
    TOPIC_CONFIG,
    TOPIC_NUM,
}TOPIC_TYPE_M;









void usr_mqtt_start();
int usr_mqtt_pulish_msg(char *topic, char *msg);
void http_native_request(char *url, char *reponse, uint32_t reponse_len);
extern char *speaker_mqtt_topic[TOPIC_NUM];



#endif /* __USR_MQTT_H__ */
