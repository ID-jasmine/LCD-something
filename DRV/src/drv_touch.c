#include "drv_touch.h"
#include "main.h"
#include "gpio.h"

#define READ_TOUCH_PIN()    Gpio_GetInputIO(GpioPortC, GpioPin0)

/* 定义消抖时间 (单位: ms)。触摸芯片一般信号较好，20ms通常足够 */
#define TOUCH_DEBOUNCE_MS   20

/* 外部获取系统时间戳的函数声明 */
extern uint32_t Get_SystemMs(void);

/* 触摸按键控制结构体 */
typedef struct {
    uint8_t raw_state;          // 引脚原始状态
    uint8_t stable_state;       // 消抖后的稳定状态
    uint32_t last_debounce_time;// 上次状态发生改变的时间戳
    uint32_t press_start_time;  // 按下开始的时间戳
    bool long_press_triggered;  // 是否已触发过长按
    Touch_Event_t event;        // 当前的按键事件
} Touch_Ctrl_t;

static Touch_Ctrl_t touch_k1;

void DRV_Touch_Init(void) {
    // 假设 GPIO PC0 的初始化已经在主函数中完成
    
    // 根据原理图，默认输出高电平，所以初始状态设为 1
    touch_k1.raw_state = 1;
    touch_k1.stable_state = 1;
    touch_k1.last_debounce_time = 0;
    touch_k1.press_start_time = 0;
    touch_k1.long_press_triggered = false;
    touch_k1.event = TOUCH_EVENT_NONE;
}

void DRV_Touch_Task(void) {
    // 1. 读取当前引脚电平
    uint8_t current_reading = READ_TOUCH_PIN();
    uint32_t current_time = Get_SystemMs();

    // 2. 状态消抖逻辑
    if (current_reading != touch_k1.raw_state) {
        touch_k1.last_debounce_time = current_time;
        touch_k1.raw_state = current_reading;
    }

    if ((current_time - touch_k1.last_debounce_time) >= TOUCH_DEBOUNCE_MS) {
        if (current_reading != touch_k1.stable_state) {
            touch_k1.stable_state = current_reading;

            // 触摸时输出低电平(0)，未触摸输出高电平(1)
            if (touch_k1.stable_state == 0) {
                // 刚按下：记录起始时间，发送基础按下事件
                touch_k1.event = TOUCH_EVENT_PRESSED;
                touch_k1.press_start_time = current_time;
                touch_k1.long_press_triggered = false;
            } else {
                // 刚释放：发送基础释放事件
                touch_k1.event = TOUCH_EVENT_RELEASED;
                
                // 如果直到释放时都没有触发长按，则判定为短按
                if (!touch_k1.long_press_triggered) {
                    uint32_t duration = current_time - touch_k1.press_start_time;
                    if (duration >= 20 && duration < 1000) { // 20ms - 1s 定义为有效短按
                        touch_k1.event = TOUCH_EVENT_SHORT;
                    }
                }
            }
        }
    }

    // 3. 长按判定逻辑 (在按下状态下持续计时)
    if (touch_k1.stable_state == 0 && !touch_k1.long_press_triggered) {
        if ((current_time - touch_k1.press_start_time) >= 3000) {
            touch_k1.long_press_triggered = true;
            touch_k1.event = TOUCH_EVENT_LONG;
        }
    }
}

Touch_Event_t DRV_Touch_GetEvent(void) {
    // 提取当前事件，并将其重置为 NONE（读后清零机制，防止主函数重复处理同一个事件）
    Touch_Event_t evt = touch_k1.event;
    touch_k1.event = TOUCH_EVENT_NONE;
    return evt;
}

bool DRV_Touch_IsPressed(void) {
    // 实时返回是否处于按下状态 (稳定状态为0则代表按下)
    return (touch_k1.stable_state == 0);
}
