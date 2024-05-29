#include "usr_mqtt_msg.h"
#include "usr_common.h"
#include "usr_audio.h"
#include "cJSON.h"


static const char *TAG = "USR_MQTT_MSG";

/*开始会话 设备发送*/
esp_err_t speaker_start_talk(char *filename, char *text)
{
    int ret = ESP_OK;

    cJSON *start_talk = cJSON_CreateObject();
    if (NULL == start_talk)
    {
        ESP_LOGE(TAG, "start_talk erro\n");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(start_talk, "event", "startSpeak");

    cJSON *body = cJSON_CreateObject();
    if (NULL == body)
    {
        ESP_LOGE(TAG, "start_talk body error\n");
        ret = ESP_FAIL;
        goto exit;
    }
    
    cJSON_AddItemToObject(start_talk, "data", body);
    
    cJSON_AddStringToObject(body, "mac", device_info.mqtt.clientId);
    cJSON_AddStringToObject(body, "filename", filename);
    cJSON_AddNumberToObject(body, "sessionId", ++(device_info.talk.sessionId));
    cJSON_AddStringToObject(body, "text", text);
    // cJSON_AddNumberToObject(body, "id", device_info.talk.roleId);
    cJSON_AddStringToObject(body, "id", speaker_mode_audio_id[device_info.talk.modeId]);
    
    char *p_tmp = cJSON_PrintUnformatted(start_talk);
    // printf("p_tmp:%s\r\n",p_tmp);
    ret = usr_mqtt_pulish_msg(speaker_mqtt_topic[TOPIC_EVENT], p_tmp);

    exit:
        cJSON_Delete(start_talk);
        return ret;
}

/*停止会话 设备发送*/
esp_err_t speaker_stop_talk(void)
{
    int ret = ESP_OK;

    cJSON *stop_talk = cJSON_CreateObject();
    if (NULL == stop_talk)
    {
        ESP_LOGE(TAG, "stop_talk erro\n");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(stop_talk, "event", "stopSpeak");

    cJSON *body = cJSON_CreateObject();
    if (NULL == body)
    {
        ESP_LOGE(TAG, "stop_talk body error\n");
        ret = ESP_FAIL;
        goto exit;
    }
    
    cJSON_AddItemToObject(stop_talk, "data", body);
    
    // cJSON_AddStringToObject(body, "mac", device_info.mqtt.clientId);
    cJSON_AddNumberToObject(body, "sessionId", device_info.talk.sessionId);

    char *p_tmp = cJSON_PrintUnformatted(stop_talk);
    printf("p_tmp:%s\r\n",p_tmp);
    ret = usr_mqtt_pulish_msg(speaker_mqtt_topic[TOPIC_EVENT], p_tmp);

    exit:
        cJSON_Delete(stop_talk);
        return ret;
}

/*更新初始化角色 设备发送*/
esp_err_t speaker_talk_role_init(void)
{
    int ret = ESP_OK;

    cJSON *talk_role_init = cJSON_CreateObject();
    if (NULL == talk_role_init)
    {
        ESP_LOGE(TAG, "talk_role_init erro\n");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(talk_role_init, "event", "updateInitPrompt");

    cJSON *body = cJSON_CreateObject();
    if (NULL == body)
    {
        ESP_LOGE(TAG, "talk_role_init body error\n");
        ret = ESP_FAIL;
        goto exit;
    }
    
    cJSON_AddItemToObject(talk_role_init, "data", body);
    
    cJSON_AddStringToObject(body, "mac", device_info.mqtt.clientId);
    
    char *p_tmp = cJSON_PrintUnformatted(talk_role_init);
    // printf("p_tmp:%s\r\n",p_tmp);
    ret = usr_mqtt_pulish_msg(speaker_mqtt_topic[TOPIC_EVENT], p_tmp);

    exit:
        cJSON_Delete(talk_role_init);
        return ret;
}

esp_err_t mqtt_username_passwd_get(char *json, char *mqtt_username, char *mqtt_passwd)
{
    int ret = ESP_OK;
    cJSON *mqtt_passwd_cmd = cJSON_Parse(json);
    if (NULL == mqtt_passwd_cmd)
    {
        ESP_LOGE(TAG, "url Parse erro\n");
        return ESP_FAIL;
    }
    if (cJSON_GetObjectItem(mqtt_passwd_cmd, "code")->valueint != 200){
        ESP_LOGE(TAG, "code error\n");
        ret = ESP_FAIL;
        goto exit;
    }
    cJSON *cmd_body = cJSON_GetObjectItem(mqtt_passwd_cmd, "data");
    if (NULL == cmd_body)
    {
        ESP_LOGE(TAG, "get body erro\n");
        ret = ESP_FAIL;
        goto exit;
    }
    if(cJSON_GetObjectItem(cmd_body, "username")->valuestring == NULL)
    {
        ESP_LOGE(TAG, "username erro\n");
        ret = ESP_FAIL;
        goto exit;
    }
    
    strcpy(mqtt_username, cJSON_GetObjectItem(cmd_body, "username")->valuestring);

    if(cJSON_GetObjectItem(cmd_body, "password")->valuestring == NULL)
    {
        ESP_LOGE(TAG, "password erro\n");
        ret = ESP_FAIL;
        goto exit;
    }

    strcpy(mqtt_passwd, cJSON_GetObjectItem(cmd_body, "password")->valuestring);

    exit:
        cJSON_Delete(mqtt_passwd_cmd);
        return ret;
}

esp_err_t mode_audio_init(char *json)
{
    int ret = ESP_OK;
    cJSON *mode_audio_cmd = cJSON_Parse(json);
    if (NULL == mode_audio_cmd)
    {
        ESP_LOGE(TAG, "url Parse erro\n");
        return ESP_FAIL;
    }
    if (cJSON_GetObjectItem(mode_audio_cmd, "code")->valueint != 200){
        ESP_LOGE(TAG, "code error\n");
        ret = ESP_FAIL;
        goto exit;
    }

    // 获取 initPrompt 数组
    cJSON *initPrompt = cJSON_GetObjectItem(cJSON_GetObjectItem(mode_audio_cmd, "data"), "initPrompt");

    // 循环遍历 initPrompt 数组
    cJSON *item;
    uint8_t audio_id=0;
    cJSON_ArrayForEach(item, initPrompt) {
        // 获取 id
        const char *id = cJSON_GetObjectItem(item, "id")->valuestring;
        if(speaker_mode_audio_id[audio_id]!=NULL){
            free(speaker_mode_audio_id[audio_id]);
        }
        speaker_mode_audio_id[audio_id] = malloc(strlen(cJSON_GetObjectItem(item, "id")->valuestring) + 1);
        strcpy(speaker_mode_audio_id[audio_id], id);
        // 获取 initAudioUrl
        const char *initAudioUrl = cJSON_GetObjectItem(item, "initAudioUrl")->valuestring;
        if(speaker_mode_audio_url[audio_id]!=NULL){
            free(speaker_mode_audio_url[audio_id]);
        }
        speaker_mode_audio_url[audio_id] = malloc(strlen(cJSON_GetObjectItem(item, "initAudioUrl")->valuestring) + 1);
        strcpy(speaker_mode_audio_url[audio_id], initAudioUrl);
        // 打印 id 和 initAudioUrl
        // printf("ID: %s\n", id);
        // printf("InitAudioUrl: %s\n", initAudioUrl);
        audio_id ++;
        device_info.talk.modeMaxId = audio_id;
        if(device_info.talk.modeMaxId > MAX_MODE_ID)
        {
            ret = ESP_FAIL;
            goto exit;
        }
    }

    exit:
        cJSON_Delete(mode_audio_cmd);
        return ret;
}

esp_err_t get_upload_music_url_json(char *json, char *url, char *text)
{
    int ret = ESP_OK;
    cJSON *url_cmd = cJSON_Parse(json);
    if (NULL == url_cmd)
    {
        ESP_LOGE(TAG, "url Parse erro\n");
        return ESP_FAIL;
    }
    if (cJSON_GetObjectItem(url_cmd, "code")->valueint != 200){
        ESP_LOGE(TAG, "code error\n");
        ret = ESP_FAIL;
        goto exit;
    }
    cJSON *cmd_body = cJSON_GetObjectItem(url_cmd, "data");
    if (NULL == cmd_body)
    {
        ESP_LOGE(TAG, "get body erro\n");
        ret = ESP_FAIL;
        goto exit;
    }

    strcpy(url, cJSON_GetObjectItem(cmd_body, "filename")->valuestring);
    strcpy(text, cJSON_GetObjectItem(cmd_body, "text")->valuestring);

    exit:
        cJSON_Delete(url_cmd);
        return ret;
}

int config_cmd_handle(char *json)
{
    int ret=ESP_OK;

    cJSON *config_cmd = cJSON_Parse(json);
    if (NULL == config_cmd)
    {
        ESP_LOGE(TAG, "create config erro\n");
        return -1;
    }

    ret = cJSON_GetObjectItem(config_cmd, "mid")->valueint;
    ESP_LOGI(TAG, ">>>> config cmd mid: %d", ret);

    cJSON *cmd_body = cJSON_GetObjectItem(config_cmd, "body");
    if (NULL == cmd_body)
    {
        ESP_LOGE(TAG, "get body erro\n");
        ret = -1;
        goto exit;
    }
     char *cmd_type = cmd_body->child->string;

    if (!strcmp(cmd_type, "mqtt"))
    {
        cJSON *mqtt_ser = cJSON_GetObjectItem(cmd_body, "mqtt");
        if (NULL == mqtt_ser)
        {
            ESP_LOGE(TAG, "get body erro\n");
            ret = -1;
            goto exit;
        }
        // usr_log(">>>>> %s", cJSON_GetObjectItem(mqtt_ser, "addr")->valuestring);

        // config_cmd_rsp("mqtt.svr", mid, 0);
        // usr_mqtt_switch_server(
        //     cJSON_GetObjectItem(mqtt_ser, "addr")->valuestring,
        //     cJSON_GetObjectItem(mqtt_ser, "port")->valuestring,
        //     cJSON_GetObjectItem(mqtt_ser, "username")->valuestring,
        //     cJSON_GetObjectItem(mqtt_ser, "passwd")->valuestring); 
    }

    exit:
        cJSON_Delete(config_cmd);
        return ret;
}

int audio_control_handle(char *json, char *url, uint32_t *sessionId, uint32_t *audioNum)
{
    int ret=ESP_OK;

    cJSON *control_cmd = cJSON_Parse(json);
    if (NULL == control_cmd)
    {
        ESP_LOGE(TAG, "create control erro\n");
        return ESP_FAIL;
    }

    char *audio_url = cJSON_GetObjectItem(control_cmd, "audioUrl")->valuestring;
    if(NULL == audio_url)
    {
        ESP_LOGE(TAG, "get audioUrl erro\n");
        ret = ESP_FAIL;
        goto exit;
    }
    strcpy(url, audio_url);
    *sessionId = cJSON_GetObjectItem(control_cmd, "sessionId")->valueint;
    *audioNum = cJSON_GetObjectItem(control_cmd, "audioNum")->valueint;
    printf("**********url:%s sessionId:%ld audioNum:%ld**************\r\n",url, *sessionId, *audioNum);
    
    exit:
        cJSON_Delete(control_cmd);
        return ret;
}

int get_audio_control_sessionId(char *json)
{
    int sessionId=ESP_OK;

    cJSON *control_cmd = cJSON_Parse(json);
    if (NULL == control_cmd)
    {
        ESP_LOGE(TAG, "create control erro\n");
        return ESP_FAIL;
    }

    char *audio_url = cJSON_GetObjectItem(control_cmd, "audioUrl")->valuestring;
    if(NULL == audio_url)
    {
        ESP_LOGE(TAG, "get audioUrl erro\n");
        sessionId = ESP_FAIL;
        goto exit;
    }
    sessionId = cJSON_GetObjectItem(control_cmd, "sessionId")->valueint;
    
    exit:
        cJSON_Delete(control_cmd);
        return sessionId;
}

esp_err_t mqtt_subscribe_msg_precess(char *topic, int topic_len, char*data, int data_len)
{
    // printf("TOPIC=%.*s\r\n", topic_len, topic);
    // printf("DATA=%.*s\r\n", data_len, data);
    
    if(!strncmp(topic, speaker_mqtt_topic[TOPIC_CONTROL], topic_len)){
        uint32_t sessionId = get_audio_control_sessionId(data);
        if(sessionId == device_info.talk.sessionId)
        {
            xQueueSendToFront(audio_play_queue, (char*)data,(TickType_t)0);
        }
    }else if(!strncmp(topic, speaker_mqtt_topic[TOPIC_CONFIG], topic_len)){
        config_cmd_handle(data);
    }

    return ESP_OK;
}


















