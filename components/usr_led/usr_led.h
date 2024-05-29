#ifndef __USR_LED_H__
#define __USR_LED_H__
#include "usr_common.h"

typedef enum
{
    //错误
    MODE_ERROR = 0,
    // 初始化,等待网络连接
    MODE_INIT,
    // 正常工作
    MODE_WORK,
    // 配网
    MODE_SMART_CONFIG,
    // 充电中
    MODE_BAT_CHARGING,

    MODE_MAX
} LED_MODE_M;

typedef enum
{
    STATE_OPEN = 0,
    STATE_CLOSE
} LED_STATE_M;

typedef struct
{
    LED_MODE_M mode;
    LED_STATE_M status;
} LED_CTRL_T;




void led_control_init(void);
void led_mode_switch(LED_CTRL_T *led, LED_MODE_M staus, bool sw);
extern LED_CTRL_T led_ctrl;

#endif /* __USR_LED_H__ */
