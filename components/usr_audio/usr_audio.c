#include "usr_audio.h"
#include "usr_common.h"

#include "audio_common.h"
#include "board.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"

#include "audio_mem.h"

#include "audio_idf_version.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"

#include "http_stream.h"
#include "i2s_stream.h"
#include "esp_http_client.h"
#include "wav_decoder.h"
#include "input_key_service.h"
#include "usr_led.h"

static const char *TAG = "USR_AUDIO";

audio_pipeline_handle_t pipeline, pipeline_rec;
audio_element_handle_t http_stream_reader, i2s_stream_writer, wav_decoder;
audio_element_handle_t i2s_stream_reader, http_stream_writer;
audio_board_handle_t board_handle;
esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
esp_periph_set_handle_t set;
QueueHandle_t audio_play_queue;

uint8_t is_audioFinish = 1;

#define EXAMPLE_AUDIO_SAMPLE_RATE  (16000)
#define EXAMPLE_AUDIO_BITS         (16)
#define EXAMPLE_AUDIO_CHANNELS     (1)



char *speaker_mode_audio_url[MAX_MODE_ID];
char *speaker_mode_audio_id[MAX_MODE_ID];


void audio_play(void *pv)
{
    char json[512] = {0};
    char url[512] = {0};
    uint32_t sessionId=0;
    uint32_t audioNum=0;
    while(1)
    {
        if(is_audioFinish)
        {
            if(xQueueReceive(audio_play_queue, &json , pdMS_TO_TICKS(20))){ 
                audio_control_handle(json, url, &sessionId, &audioNum);
                play_audio_set_url(url);
                is_audioFinish = 0;
            }
        }
        else
        {
            vTaskDelay( pdMS_TO_TICKS(100));
        }
        
    }
}

void set_audio_mode_id(DEVICE_INFO_T *dev)
{
    dev->talk.modeId += 1;
    if (dev->talk.modeId >= dev->talk.modeMaxId) {
        dev->talk.modeId = 0;
    }
}

void mode_audio_timed_request(void *pv)
{
    uint32_t count=0;
    while(1)
    {
        while((periph_wifi_is_connected(wifi_handle)!=PERIPH_WIFI_CONNECTED) && (periph_wifi_is_connected(wifi_handle)!=PERIPH_WIFI_CONFIG_DONE))
        {
            ESP_LOGI(TAG, "mode_audio_timed_request state %d wait wifi connect ...", periph_wifi_is_connected(wifi_handle));
            vTaskDelay( pdMS_TO_TICKS(2000));
            led_mode_switch(&led_ctrl,MODE_INIT, false);
        }
        if(count>=240){
            char *reponse = malloc(1024*10*sizeof(char));
            http_native_request(device_info.http.get_url, reponse, 1024*10);
            mode_audio_init(reponse);
            free(reponse);
            count=0;
            for(uint8_t i=0;i<device_info.talk.modeMaxId;i++)
            {
                printf("id:%s url%s\r\n",speaker_mode_audio_id[i],speaker_mode_audio_url[i]);
            }
        }else{
            count++;
        }
        vTaskDelay( pdMS_TO_TICKS(1000));
    }
}

esp_err_t _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    char len_buf[16];
    static int total_write = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // set header
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        char dat[1024] = {0};
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_SAMPLE_RATE);
        esp_http_client_set_header(http, "x-audio-sample-rates", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_BITS);
        esp_http_client_set_header(http, "x-audio-bits", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%d", EXAMPLE_AUDIO_CHANNELS);
        esp_http_client_set_header(http, "x-audio-channel", dat);
        memset(dat, 0, sizeof(dat));
        snprintf(dat, sizeof(dat), "%s", device_info.mqtt.passwd);
        esp_http_client_set_header(http, "Authorization", dat);
        total_write = 0;
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
        // write data
        int wlen = sprintf(len_buf, "%x\r\n", msg->buffer_len);
        if (esp_http_client_write(http, len_buf, wlen) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, msg->buffer, msg->buffer_len) <= 0) {
            return ESP_FAIL;
        }
        if (esp_http_client_write(http, "\r\n", 2) <= 0) {
            return ESP_FAIL;
        }
        total_write += msg->buffer_len;
        printf("\033[A\33[2K\rTotal bytes written: %d\n", total_write);
        return msg->buffer_len;
    }

    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
        if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST");
        char *buf = calloc(1, 256);
        // assert(buf);
        int read_len = esp_http_client_read(http, buf, 256);
        if (read_len <= 0) {
            free(buf);
            return ESP_FAIL;
        }
        buf[read_len] = 0;
        ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)buf);

        char url[256] = {0};
        char text[1024] = {0};
        get_upload_music_url_json(buf,url,text);
        // printf("url:%s\r\n",url);
        speaker_start_talk(url, text);
        free(buf);
        return ESP_OK;
    }
    return ESP_OK;
}


void play_audio_set_url(char *url)
{
    audio_element_set_uri(http_stream_reader, url);
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_element_reset_state(i2s_stream_writer);
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);
    audio_pipeline_run(pipeline);
}

esp_err_t audio_element_event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx) {
    ESP_LOGI(TAG, "Audio event %d from %s element", event->cmd, audio_element_get_tag(self));
    if (event->cmd == AEL_MSG_CMD_REPORT_POSITION) {
        is_audioFinish = 1;
        printf("单条播放完毕\r\n");
    }
    return ESP_OK;
}

void set_audio_volume(audio_hal_handle_t audio_hal, DEVICE_INFO_T *dev, bool act)
{
    if(true == act)
    {
        dev->volume += 10;
        if (dev->volume > 100) {
            dev->volume = 100;
        }
    }else{
        dev->volume -= 10;
        if (dev->volume < 0) {
            dev->volume = 0;
        }
    }
    audio_hal_set_volume(audio_hal, dev->volume);
    int volume=0;
    audio_hal_get_volume(audio_hal, &volume);
    printf("volume set %d\r\n",volume);
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    audio_element_handle_t usr_http_stream_writer = (audio_element_handle_t)ctx;
    if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_MODE:
                printf("mode btn press\r\n");
                
                break;
            case INPUT_KEY_USER_ID_REC:
                printf("rec btn press\r\n");
                speaker_stop_talk();
                xQueueReset(audio_play_queue);
                printf("rec btn press\r\n");
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                audio_pipeline_terminate(pipeline);
                audio_pipeline_stop(pipeline_rec);
                audio_pipeline_wait_for_stop(pipeline_rec);
                audio_pipeline_reset_ringbuffer(pipeline_rec);
                audio_pipeline_reset_elements(pipeline_rec);
                audio_pipeline_terminate(pipeline_rec);

                audio_element_set_uri(usr_http_stream_writer, "http://ai-test-api.jeemoo.net/py/mqtt/upload");
                audio_pipeline_run(pipeline_rec);
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                printf("voldown btn press\r\n");
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                printf("volup btn press\r\n");
                break;
        }
    } else if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS_RELEASE) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
                printf("rec btn release\r\n");
                audio_element_set_ringbuf_done(i2s_stream_reader);
                break;
            case INPUT_KEY_USER_ID_MODE:
                printf("mode btn release\r\n");
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                printf("voldown btn release\r\n");
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                printf("volup btn release\r\n");
                break;
        }
    }

    return ESP_OK;
}

void audio_process(void *pv)
{
    ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);
    printf("player_volume set %d\r\n",player_volume);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);
    mem_assert(pipeline_rec);

    ESP_LOGI(TAG, "[2.1] Create http stream to read data");
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.out_rb_size = 24 * 1024;
    http_stream_reader = http_stream_init(&http_cfg);

     ESP_LOGI(TAG, "[2.3] Create mp3 decoder to decode mp3 file");
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_cfg.out_rb_size = 24 * 1024;
    wav_decoder = wav_decoder_init(&wav_cfg);
    
    ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, EXAMPLE_AUDIO_SAMPLE_RATE, EXAMPLE_AUDIO_BITS, AUDIO_STREAM_WRITER);
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.out_rb_size = 24 * 1024;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, wav_decoder,        "wav");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    ESP_LOGI(TAG, "[3.1] Create http stream to post data to server");
    http_stream_cfg_t http_cfg_writer = HTTP_STREAM_CFG_DEFAULT();
    http_cfg_writer.type = AUDIO_STREAM_WRITER;
    http_cfg_writer.out_rb_size = 16 * 1024;
    http_cfg_writer.event_handle = _http_stream_event_handle;
    http_stream_writer = http_stream_init(&http_cfg_writer);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 44100, 16, AUDIO_STREAM_READER);
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    i2s_cfg_read.out_rb_size = 16 * 1024; // Increase buffer to avoid missing data in bad network conditions
    i2s_cfg_read.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_rec, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline_rec, http_stream_writer, "http");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]");
    const char *link_tag_rec[2] = {"i2s", "http"};
    audio_pipeline_link(pipeline_rec, &link_tag_rec[0], 2);



    ESP_LOGI(TAG, "[2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"http", "wav", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);


    esp_err_t  ret = audio_board_key_init(set);
    printf("ret = %d\r\n",ret);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)http_stream_writer);

    audio_element_set_event_callback(i2s_stream_writer, audio_element_event_handler, NULL);

    // periph_button_cfg_t btn_cfg = {
    //     .gpio_mask = ((1ULL << get_input_rec_id()) | (1ULL << get_input_mode_id()) | (1ULL << get_input_volup_id()) | (1ULL << get_input_voldown_id())), //REC BTN & MODE BTN
    //     .long_press_time_ms = 2000,
    // };
    // esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    ESP_LOGI(TAG, "[3.2] Start all peripherals");
    // esp_periph_start(set, button_handle);

    i2s_stream_set_clk(i2s_stream_reader, EXAMPLE_AUDIO_SAMPLE_RATE, EXAMPLE_AUDIO_BITS, EXAMPLE_AUDIO_CHANNELS);
    i2s_stream_set_clk(i2s_stream_writer, EXAMPLE_AUDIO_SAMPLE_RATE, EXAMPLE_AUDIO_BITS, EXAMPLE_AUDIO_CHANNELS);

    // Example of using an audio event -- START
    // ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    // audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    // audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    // ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    // audio_pipeline_set_listener(pipeline, evt);

    // ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    // audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    audio_play_queue= xQueueCreate(10, 512*sizeof(char));

    xTaskCreatePinnedToCore(audio_play, "audio_play",  4*1024, NULL, 12, NULL, 1);

    xTaskCreatePinnedToCore(mode_audio_timed_request, "mode_audio_timed_request",  4*1024, NULL, 8, NULL, 0);

    while (1) {
        vTaskDelay( pdMS_TO_TICKS(2000));
        // audio_event_iface_msg_t msg;
        // esp_err_t ret = audio_event_iface_listen(evt, &msg, pdMS_TO_TICKS(400));
        // if (ret != ESP_OK) {
        //     // ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
        //     continue;
        // }

        // if ((msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
        //     && (msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {
        //     if ((int) msg.data == get_input_rec_id()) {
        //         speaker_stop_talk();
        //         xQueueReset(audio_play_queue);
        //         printf("rec btn press\r\n");
        //         audio_pipeline_stop(pipeline);
        //         audio_pipeline_wait_for_stop(pipeline);
        //         audio_pipeline_reset_ringbuffer(pipeline);
        //         audio_pipeline_reset_elements(pipeline);
        //         audio_pipeline_terminate(pipeline);
        //         audio_pipeline_stop(pipeline_rec);
        //         audio_pipeline_wait_for_stop(pipeline_rec);
        //         audio_pipeline_reset_ringbuffer(pipeline_rec);
        //         audio_pipeline_reset_elements(pipeline_rec);
        //         audio_pipeline_terminate(pipeline_rec);

        //         audio_element_set_uri(http_stream_writer, "http://ai-test-api.jeemoo.net/py/mqtt/upload");
        //         audio_pipeline_run(pipeline_rec);
        //     }else if((int) msg.data == get_input_volup_id()){
        //         printf("volup btn press\r\n");
        //         set_audio_volume(board_handle->audio_hal, &device_info, true);
        //     }else if((int) msg.data == get_input_voldown_id()){
        //         printf("voldown btn press\r\n");
        //         set_audio_volume(board_handle->audio_hal, &device_info, false);
        //     }else if((int) msg.data == get_input_mode_id()){
        //         set_audio_mode_id(&device_info);
        //         printf("mode btn press modeIndex:%d/%d modeId:%s\r\n",device_info.talk.modeId, device_info.talk.modeMaxId-1, speaker_mode_audio_id[device_info.talk.modeId]);
        //     }
        //     continue;
        // }
    
        // if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
        //     && (msg.cmd == PERIPH_ADC_BUTTON_RELEASE)) {
        //     if ((int) msg.data == get_input_rec_id()) {
        //         printf("rec btn release\r\n");
        //         audio_element_set_ringbuf_done(i2s_stream_reader);
        //     }
        //     continue;  
        // }

        // if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
        //     && (msg.cmd == PERIPH_ADC_BUTTON_LONG_PRESSED)) {
        //     if((int) msg.data == get_input_mode_id()){
        //         led_mode_switch(&led_ctrl,MODE_SMART_CONFIG,false);
        //         periph_wifi_config_start(wifi_handle, WIFI_CONFIG_ESPTOUCH);
        //         esp_err_t err = periph_wifi_config_wait_done(wifi_handle, portMAX_DELAY);
        //         if(err != ESP_OK){
        //             led_mode_switch(&led_ctrl,MODE_INIT,true);
        //             break;
        //         }
        //         led_mode_switch(&led_ctrl,MODE_WORK,true);
        //         wifi_config_t w_config;
        //         esp_wifi_get_config(WIFI_IF_STA, &w_config);

        //         printf("ssid %s passwd %s is_connect %d\r\n", w_config.sta.ssid, w_config.sta.password, periph_wifi_is_connected(wifi_handle));
        //         memset(device_info.wifi.ssid, 0, 32);
        //         memset(device_info.wifi.passwd, 0, 64);
        //         memcpy(device_info.wifi.ssid, w_config.sta.ssid, sizeof(w_config.sta.ssid));
        //         memcpy(device_info.wifi.passwd, w_config.sta.password, sizeof(w_config.sta.password));
        //         printf("ssid %s passwd %s\r\n", device_info.wifi.ssid, device_info.wifi.passwd);
        //         write_wifi_info(&device_info.wifi);
        //     }
        //     continue;  
        // }
    }
    // Example of using an audio event -- END

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_unregister(pipeline, http_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, wav_decoder);

    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    // audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    // audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(wav_decoder);
    esp_periph_set_destroy(set);

    vTaskDelete(NULL);
}

void wifi_start(void *pv)
{
    set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {0};
    memcpy(wifi_cfg.wifi_config.sta.ssid, device_info.wifi.ssid, strlen(device_info.wifi.ssid));
    memcpy(wifi_cfg.wifi_config.sta.password, device_info.wifi.passwd, strlen(device_info.wifi.passwd));
    wifi_handle = periph_wifi_init(&wifi_cfg);
    
    esp_periph_start(set, wifi_handle);
    
    while(periph_wifi_is_connected(wifi_handle) != PERIPH_WIFI_CONNECTED)
    {
        periph_wifi_wait_for_connected(wifi_handle, pdMS_TO_TICKS(1000));   
    }
    esp_wifi_set_ps(WIFI_PS_NONE);

    vTaskDelete(NULL);
}

void usr_wifi_start()
{
    xTaskCreatePinnedToCore(wifi_start, "wifi_start",  10*1024, NULL, 8, NULL, 0);
}

void usr_audio_start()
{
    vTaskDelay( pdMS_TO_TICKS(2000));
    xTaskCreatePinnedToCore(audio_process, "audio_process",  10*1024, NULL, 15, NULL, 1);
}







