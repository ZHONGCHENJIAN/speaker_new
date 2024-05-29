#include "usr_mqtt.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "usr_audio.h"
#include "usr_led.h"
static const char *TAG = "USR_MQTT";

char *speaker_mqtt_topic[TOPIC_NUM];
esp_mqtt_client_handle_t mqtt_client;

void speaker_mqtt_topic_init()
{
    char topic[64] = {0};
    sprintf(topic, "speaker.event");
    speaker_mqtt_topic[TOPIC_EVENT] = malloc(strlen(topic)+1);
    strcpy(speaker_mqtt_topic[TOPIC_EVENT], topic);

    memset(topic, 0, 64);
    sprintf(topic, "speaker.%s.control", device_info.mqtt.clientId);
    speaker_mqtt_topic[TOPIC_CONTROL] = malloc(strlen(topic)+1);
    strcpy(speaker_mqtt_topic[TOPIC_CONTROL], topic);

    memset(topic, 0, 64);
    sprintf(topic, "speaker.%s.config", device_info.mqtt.clientId);
    speaker_mqtt_topic[TOPIC_CONFIG] = malloc(strlen(topic)+1);
    strcpy(speaker_mqtt_topic[TOPIC_CONFIG], topic);

    printf("TOPIC_EVENT:%s TOPIC_CONTROL:%s TOPIC_CONFIG:%s\r\n",speaker_mqtt_topic[TOPIC_EVENT],
                                                            speaker_mqtt_topic[TOPIC_CONTROL],
                                                            speaker_mqtt_topic[TOPIC_CONFIG]);
}

int usr_mqtt_pulish_msg(char *topic, char *msg)
{
    return esp_mqtt_client_enqueue(mqtt_client, topic, msg, 0, 1, 0, true);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, speaker_mqtt_topic[TOPIC_CONFIG], 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        
        msg_id = esp_mqtt_client_subscribe(client, speaker_mqtt_topic[TOPIC_CONTROL], 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        printf("use id:%s url:%s\r\n",speaker_mode_audio_id[device_info.talk.modeId],speaker_mode_audio_url[device_info.talk.modeId]);
        play_audio_set_url(speaker_mode_audio_url[device_info.talk.modeId]);
        led_mode_switch(&led_ctrl,MODE_WORK,false);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        led_mode_switch(&led_ctrl,MODE_INIT,false);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        mqtt_subscribe_msg_precess(event->topic, event->topic_len, event->data, event->data_len);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void http_native_request(char *url, char *reponse, uint32_t reponse_len)
{
    // Declare local_response_buffer with size (MAX_HTTP_OUTPUT_BUFFER + 1) to prevent out of bound access when
    // it is used by functions like strlen(). The buffer should only be used upto size MAX_HTTP_OUTPUT_BUFFER
    int content_length = 0;
    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, reponse, reponse_len);
            reponse[data_read]=0;
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                printf("%s\r\n",reponse);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

static void mqtt_start(void *pv)
{
reload:
    while(periph_wifi_is_connected(wifi_handle) != PERIPH_WIFI_CONNECTED)
    {
        ESP_LOGI(TAG, "state %d wait wifi connect ...", periph_wifi_is_connected(wifi_handle));
        vTaskDelay( pdMS_TO_TICKS(1000));
    }

    char *reponse = malloc(1024*10*sizeof(char));
    if(NULL == reponse)
    {
        goto reload;
    }
    http_native_request(device_info.http.get_url, reponse, 1024*10);
    esp_err_t rec = mqtt_username_passwd_get(reponse, device_info.mqtt.userName, device_info.mqtt.passwd);
    if(rec!=ESP_OK){
        free(reponse);
        goto reload;
    }
    mode_audio_init(reponse);
    for(uint8_t i=0;i<device_info.talk.modeMaxId;i++)
    {
        printf("id:%s url:%s\r\n",speaker_mode_audio_id[i],speaker_mode_audio_url[i]);
    }
    free(reponse);
    speaker_mqtt_topic_init();
    
    char url[64];
    sprintf(url, "mqtt://%s:%d",device_info.mqtt.ip, device_info.mqtt.port);
    printf("url:%s\r\n", url);
    printf("userName:%s\r\n", device_info.mqtt.userName);
    printf("clientId:%s\r\n", device_info.mqtt.clientId);
    printf("passwd:%s\r\n", device_info.mqtt.passwd);
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = url,
        .credentials.username = device_info.mqtt.userName,
        .credentials.client_id = device_info.mqtt.clientId,
        .credentials.authentication.password = device_info.mqtt.passwd,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    /* Now we start the client and it's possible to see the memory usage for the operations in the outbox. */
    esp_mqtt_client_start(mqtt_client);

    vTaskDelete(NULL);
}

void usr_mqtt_start()
{
    xTaskCreatePinnedToCore(mqtt_start, "mqtt_start",  10*1024, NULL, 6, NULL, 0);
}
