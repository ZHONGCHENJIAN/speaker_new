#include "usr_led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
// LED逻辑
// 控制逻辑：IO引脚初始为低电平，点亮灯珠为高电平点亮
// GREEN:   GPIO22
// RED      GPIO9
// BLUE     GPIO10

// LED需要和充电做关联，正常开机后亮蓝灯，充电中信号出发后亮绿灯闪烁间隔500ms，充电完成绿灯常亮，红灯为低电量告警

#define GREEN_LED_GPIO            GPIO_NUM_22
// #define RED_LED_GPIO              GPIO_NUM_3
// #define BLUE_LED_GPIO             GPIO_NUM_1

#define MAX_DATA_MODE_COUNT (5)
#define MIN_DATA_MODE_COUNT (0)
#define INIT_FLASH_TIME_MS (200)
#define DATA_FLASH_TIME_MS (200)
#define BAT_FLASH_TIME_MS (500)
#define BAT_FLASH_COUNT (3)

LED_CTRL_T led_ctrl = {0};

void led_mode_switch(LED_CTRL_T *led, LED_MODE_M staus, bool sw)
{   
    if(led->mode == MODE_SMART_CONFIG && sw == false)
    {
        return;
    }
    led->mode = staus;
}

static LED_MODE_M get_led_mode(LED_CTRL_T *led)
{
    return led->mode;
}

static LED_STATE_M led_state_switch(LED_CTRL_T *led)
{
    led->status = !led->status;
    return led->status;
}

void led_init(gpio_num_t gpio)
{
     gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL << gpio);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

void led_pin_ctrl(uint8_t sw)
{
    gpio_set_level(GREEN_LED_GPIO, sw);
}

static void led_control(void *param)
{
    led_init(GREEN_LED_GPIO);
    // led_init(RED_LED_GPIO);
    // led_init(BLUE_LED_GPIO);
    led_mode_switch(&led_ctrl,MODE_INIT,false);
    while (1)
    {
        switch (get_led_mode(&led_ctrl))
        {
        case MODE_ERROR:
            led_pin_ctrl(false);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        case MODE_INIT:
            // 持续闪烁
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(INIT_FLASH_TIME_MS / portTICK_PERIOD_MS);
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(INIT_FLASH_TIME_MS / portTICK_PERIOD_MS);
            break;
        case MODE_WORK:
            led_pin_ctrl(true);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        case MODE_SMART_CONFIG:
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            break;
        case MODE_BAT_CHARGING:
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(BAT_FLASH_TIME_MS / portTICK_PERIOD_MS);
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(BAT_FLASH_TIME_MS / portTICK_PERIOD_MS);
            break;
        default:
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            led_pin_ctrl(led_state_switch(&led_ctrl));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        }
    }
    vTaskDelete(NULL);
}


void led_control_init(void)
{
    xTaskCreatePinnedToCore(led_control, "led_control",  4*1024, NULL, 12, NULL, 1);
}












































