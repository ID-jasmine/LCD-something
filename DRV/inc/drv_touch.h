#ifndef __DRV_TOUCH_H__
#define __DRV_TOUCH_H__

#include "ddl.h"
#include <stdint.h>
#include <stdbool.h>

/* 定义触摸按键可能产生的事件 */
typedef enum {
    TOUCH_EVENT_NONE = 0,   // 无事件
    TOUCH_EVENT_PRESSED,    // 按下事件
    TOUCH_EVENT_RELEASED,   // 松开事件
    TOUCH_EVENT_SHORT,      // 短按事件 (按下并释放)
    TOUCH_EVENT_LONG        // 长按事件 (按下持续 3s)
} Touch_Event_t;

/* 初始化触摸驱动 */
void DRV_Touch_Init(void);

/* 触摸驱动任务处理（需放在主循环中非阻塞调用） */
void DRV_Touch_Task(void);

/* 获取按键触发事件（读取后自动清除） */
Touch_Event_t DRV_Touch_GetEvent(void);

/* 获取当前按键的实时稳定状态 */
bool DRV_Touch_IsPressed(void);

#endif /* __DRV_TOUCH_H__ */
