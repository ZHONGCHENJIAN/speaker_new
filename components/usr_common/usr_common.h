#ifndef __USR_COMMON_H__
#define __USR_COMMON_H__
#include "stdbool.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_peripherals.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "usr_mqtt_msg.h"
#include "usr_mqtt.h"

#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "periph_wifi.h"

// #define WIFI_SSID "YJY_2.4G"
// #define WIFI_PASSWD "yjy88888888"
#define WIFI_SSID "TP-LINK_2.4G"
#define WIFI_PASSWD "15168306064"

#define HTTP_HOSTNAME "http://ai-test-api.jeemoo.net"
#define MQTT_IP "43.138.100.34"
#define MQTT_PORT (1884)
#define MQTT_NAME "eb_1711684697893036033"
#define MQTT_PASSWD "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJsb2dpblR5cGUiOiJ3ZWNoYXQiLCJsb2dpbklkIjoiMTcxMTY4NDY5Nzg5MzAzNjAzMyIsImRldmljZSI6IndlY2hhdCIsImVmZiI6MTcxNTMyMDMzNDM1OSwicm5TdHIiOiJwTFlWOEpIRnUyVWxkNjhiR2NlOEk0dm1TWTE1a3NPVSJ9.EDNVmX6IlH0kHmV9eL9htoxr2fxdvgUzqMUY1_l1J7M"

// #define MQTT_IP "47.98.222.34"
// #define MQTT_PORT (1883)
// #define MQTT_NAME "lease_mqtt"
// #define MQTT_PASSWD "cs32jsaH*@gt#@45G"

typedef struct
{
    char ssid[32];
    char passwd[64];
    bool isConnect;
    uint32_t crc32;
} WIFI_T;

typedef struct
{
    char ip[32];
    int port;
    char userName[32];
    char passwd[1024];
    bool isConnect;
    char clientId[32];
    char httpHostName[64];
    uint32_t crc32;
} MQTT_T;

typedef struct
{
    uint8_t btn_value;
} SMART_CONFIG_T;

typedef struct
{
    uint32_t sessionId;
    uint32_t roleId;
    uint8_t modeId;
    uint8_t modeMaxId;
}TALK_T;


typedef struct
{
    char get_url[128];
}HTTP_T;

typedef struct
{
    WIFI_T wifi;
    MQTT_T mqtt;
    SMART_CONFIG_T smart_config;
    uint8_t mac[6];
    TALK_T talk;
    HTTP_T http;
    int8_t volume;
} DEVICE_INFO_T;

void device_info_init();

esp_err_t write_wifi_info(WIFI_T *wifi_info);

extern DEVICE_INFO_T device_info;
extern esp_periph_handle_t wifi_handle;

#endif /* __USR_COMMON_H__ */