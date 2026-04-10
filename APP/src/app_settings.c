#include "app_settings.h"

#include "app_ui.h"
#include "app_vehicle.h"
#include "bsp_can.h"
#include "drv_touch.h"

#include "main.h"
#include "rtc.h"

uint16_t sys_tire_perimeter = 1619;
uint8_t sys_speed_pulse = 40;

static uint32_t setting_timer = 0; // 10s 倒计时

// 按键任务逻辑
void UI_Button_Task(void) {
    Touch_Event_t evt = DRV_Touch_GetEvent();

    // 1. 处理有按键按下的情况
    if (evt != TOUCH_EVENT_NONE) {

        // 只要在设置模式下有按键动作，刷新 10s 计时器
        if (current_display_mode == MODE_TIME_SET_HOUR ||
            current_display_mode == MODE_TIME_SET_MIN) {
            setting_timer = Get_SystemMs();
        }

        if (evt == TOUCH_EVENT_LONG) {
            if (SpeedDriver <= 1) {
                switch (current_display_mode) {
                case MODE_ODO:
                    current_unit = (current_unit == UNIT_METRIC) ? UNIT_IMPERIAL
                                                                 : UNIT_METRIC;
                    Save_Mileage_To_EEPROM();
                    break;
                case MODE_TRIP:
                    TRIP_Value = 0;
                    Save_Mileage_To_EEPROM();
                    break;
                case MODE_CLOCK:
                    current_display_mode = MODE_TIME_SET_HOUR;
                    // 进入设置模式时，必须立刻刷新计时器！防止被下面的超时逻辑秒杀
                    setting_timer = Get_SystemMs();
                    break;
                case MODE_TIME_SET_HOUR:
                    current_display_mode = MODE_TIME_SET_MIN;
                    setting_timer = Get_SystemMs(); // 切换分钟时也刷新一下
                    break;
                case MODE_TIME_SET_MIN: {
                    stc_rtc_time_t time;
                    if (Ok == Rtc_ReadDateTime(&time)) {
                        time.u8Hour = DEC2BCD(hour);
                        time.u8Minute = DEC2BCD(minute);
                        time.u8Second = 0;
                        Rtc_SetTime(&time);
                    }
                    current_display_mode = MODE_CLOCK;
                    Save_Mileage_To_EEPROM();
                    break;
                }
                default:
                    break;
                }
            }
        }

        if (evt == TOUCH_EVENT_SHORT) {
            if (can_fault_count > 0) {
                current_fault_show_idx++;
                if (current_fault_show_idx >= can_fault_count) {
                    current_fault_show_idx = 0;
                }
            } else {
                switch (current_display_mode) {
                case MODE_ODO:
                    current_display_mode = MODE_TRIP;
                    Save_Mileage_To_EEPROM();
                    break;
                case MODE_TRIP:
                    current_display_mode = MODE_CLOCK;
                    Save_Mileage_To_EEPROM();
                    break;
                case MODE_CLOCK:
                    current_display_mode = MODE_ODO;
                    Save_Mileage_To_EEPROM();
                    break;
                case MODE_TIME_SET_HOUR:
                    hour = (hour + 1) % 24;
                    break;
                case MODE_TIME_SET_MIN:
                    minute = (minute + 1) % 60;
                    break;
                default:
                    break;
                }
            }
        }
    }

    // 2. 超时判断必须放在外层！即使 evt 是 NONE 也要时刻检查是否超时
    if (current_display_mode == MODE_TIME_SET_HOUR ||
        current_display_mode == MODE_TIME_SET_MIN) {
        if (Get_SystemMs() - setting_timer >= 10000) {
            // 超时退出并自动保存当前设定的时间
            stc_rtc_time_t time;
            if (Ok == Rtc_ReadDateTime(&time)) {
                time.u8Hour = DEC2BCD(hour);
                time.u8Minute = DEC2BCD(minute);
                time.u8Second = 0;
                Rtc_SetTime(&time);
            }
            current_display_mode = MODE_CLOCK;
            Save_Mileage_To_EEPROM();
        }
    }
}
