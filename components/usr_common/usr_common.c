#include "usr_common.h"
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



#include <string.h>

esp_periph_handle_t wifi_handle = {0};
DEVICE_INFO_T device_info = {0};
#define STORAGE_NAMESPACE "storage"
#define CRC32_POLYNOMIAL 0xEDB88320L

uint32_t usr_crc32(uint8_t *data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLYNOMIAL : 0);
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

esp_err_t read_wifi_info(WIFI_T *wifi_info)
{
    nvs_handle_t wifi_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &wifi_handle);
    if (err != ESP_OK) return err;
    size_t len = sizeof(WIFI_T);
    err = nvs_get_blob(wifi_handle, "wifiwifi", wifi_info, &len);
    if (err != ESP_OK) return err;
    nvs_close(wifi_handle);
    uint32_t crc32 = usr_crc32(&device_info.wifi, sizeof(WIFI_T) - sizeof(uint32_t));
    // printf("crc32 %ld %ld",crc32,device_info.wifi.crc32);
    if(crc32 != device_info.wifi.crc32) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t write_wifi_info(WIFI_T *wifi_info)
{
    nvs_handle_t wifi_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &wifi_handle);
    if (err != ESP_OK) return err;
    wifi_info->crc32 = usr_crc32(wifi_info, sizeof(WIFI_T) - sizeof(uint32_t));
    err = nvs_set_blob(wifi_handle, "wifiwifi", wifi_info, sizeof(WIFI_T));
    if (err != ESP_OK) return err;
    err = nvs_commit(wifi_handle);
    if (err != ESP_OK) return err;
    nvs_close(wifi_handle);
    return ESP_OK;
}

esp_err_t read_mqtt_info(MQTT_T *mqtt_info)
{
    nvs_handle_t mqtt_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &mqtt_handle);
    if (err != ESP_OK) return err;
    size_t len = sizeof(MQTT_T);
    err = nvs_get_blob(mqtt_handle, "mqtt", mqtt_info, &len);
    if (err != ESP_OK) return err;
    nvs_close(mqtt_handle);
    uint32_t crc32 = usr_crc32(&device_info.mqtt, sizeof(MQTT_T) - sizeof(uint32_t));
    // printf("crc32 %ld %ld",crc32,device_info.mqtt.crc32);
    if(crc32 != device_info.mqtt.crc32) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t write_mqtt_info(MQTT_T *mqtt_info)
{
    nvs_handle_t mqtt_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &mqtt_handle);
    if (err != ESP_OK) return err;
    mqtt_info->crc32 = usr_crc32(&device_info.mqtt, sizeof(MQTT_T) - sizeof(uint32_t));
    err = nvs_set_blob(mqtt_handle, "mqtt", mqtt_info, sizeof(MQTT_T));
    if (err != ESP_OK) return err;
    err = nvs_commit(mqtt_handle);
    if (err != ESP_OK) return err;
    nvs_close(mqtt_handle);
    return ESP_OK;
}

void device_info_init()
{
    esp_err_t err = read_wifi_info(&device_info.wifi);
    if (err != ESP_OK)
    {
        printf("wifi param init...............");
        memset(&device_info.wifi, 0, sizeof(WIFI_T));
        strcpy(device_info.wifi.ssid, WIFI_SSID);
        strcpy(device_info.wifi.passwd, WIFI_PASSWD);
        device_info.wifi.crc32 = usr_crc32(&device_info.wifi, sizeof(WIFI_T) - sizeof(uint32_t));
        write_wifi_info(&device_info.wifi);
    }
    device_info.wifi.isConnect = false;
    printf("WIFI ssid:%s, passed:%s\r\n",device_info.wifi.ssid, device_info.wifi.passwd);

    err = read_mqtt_info(&device_info.mqtt);
    if (err != ESP_OK)
    {
        printf("mqtt param init.................");
        memset(&device_info.mqtt, 0, sizeof(MQTT_T));
        strcpy(device_info.mqtt.ip, MQTT_IP);
        device_info.mqtt.port = MQTT_PORT;
        strcpy(device_info.mqtt.userName, MQTT_NAME);
        strcpy(device_info.mqtt.passwd, MQTT_PASSWD);
        strcpy(device_info.mqtt.httpHostName, HTTP_HOSTNAME);
        device_info.mqtt.crc32 = usr_crc32(&device_info.mqtt, sizeof(MQTT_T) - sizeof(uint32_t));
        write_mqtt_info(&device_info.mqtt);
    }
    device_info.mqtt.isConnect = false;
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    sprintf(device_info.mqtt.clientId,"%x:%x:%x:%x:%x:%x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    printf("MQTT ip:%s, port:%d, name:%s, passed:%s, clientId:%s\r\n",device_info.mqtt.ip, 
                                                            device_info.mqtt.port,
                                                            device_info.mqtt.userName,
                                                            device_info.mqtt.passwd,
                                                            device_info.mqtt.clientId);
    sprintf(device_info.http.get_url,"%s/robot/v2/login?mac=%s",device_info.mqtt.httpHostName, device_info.mqtt.clientId);
    printf("Http get_url:%s\r\n",device_info.http.get_url);
}
















