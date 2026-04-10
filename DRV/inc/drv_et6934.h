#ifndef __DRV_ET6934_H
#define __DRV_ET6934_H

#include "bsp_et6934.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------- 枚举定义区 ------------------- */
// 独立指示灯枚举
typedef enum {
    IND_WATER_TEMP,         // 水温指示灯
    IND_FAULT,              // 故障指示灯
    IND_START_STOP,         // 启停指示灯
    IND_ENGINE_FAULT,       // 发动机故障指示灯
    IND_TURN_LEFT,          // 左转向指示灯
    IND_HIGH_BEAM,          // 远光指示灯
    IND_P_GEAR,             // P档指示灯
    IND_TCS,                // TCS指示灯
    IND_ABS,                // ABS指示灯
    IND_TURN_RIGHT,         // 右转向指示灯
    IND_BATTERY_ALARM,      // 电池报警指示灯
    IND_SPEED_KMH,          // 车速单位 km/h
    IND_SPEED_MPH,          // 车速单位 mph
    IND_ODO_MODE,           // ODO模式指示灯
    IND_TRIP_MODE,          // TRIP模式指示灯
    IND_CLOCK_MODE,         // 时钟模式指示灯
    IND_TRIP_UNIT_MILES,    // 小计单位 miles
    IND_TRIP_UNIT_KM,       // 小计单位 KM
    IND_SEC_JUMP_0,         // 秒数跳跃0
    IND_SEC_JUMP_1,         // 秒数跳跃1
    IND_BTN_SET,            // 按钮set指示灯
    IND_FUEL_WHITE,         // 油量图标白色
    IND_FUEL_YELLOW         // 油量图标黄色
} DRC_Indicator_t;

/* ------------------- API 函数区 ------------------- */

// 1. 初始化中间层 
void DRC_ET6934_Init(IIC_Handle_t* iic1, IIC_Handle_t* iic2, IIC_Handle_t* iic3);

// 2. 将内存中的显存数据统一刷新到屏幕 (建议放在定时器或主循环中周期调用)
void DRC_ET6934_Refresh(void);

// 3. 全局显示控制
void DRC_ET6934_ClearAll(void);             // 清空所有显示缓存
void DRC_ET6934_SetCanvas(bool state);      // 点亮/关闭所有静态画布（边框、单位等）

// 4. 具体元素渲染 API
void DRC_ET6934_SetIndicator(DRC_Indicator_t ind, bool state); // 控制单个指示灯
void DRC_ET6934_SetSpeed(uint16_t speed);                      // 设置车速 (例如: 120)
void DRC_ET6934_SetRPM(uint8_t rpm_level);                     // 设置转速条 (0 ~ 39)
void DRC_ET6934_SetFuel(uint8_t fuel_level);                   // 设置油量 (0 ~ 4)
void DRC_ET6934_SetBattery(uint8_t bat_level);                 // 设置电量 (0 ~ 4)
void DRC_ET6934_SetSingleDigit(uint8_t digit_idx, uint8_t value); // 设置单个数位
void DRC_ET6934_SetTrip(uint32_t trip_val);                    // 设置小计里程 (最大 999999)
void DRC_ET6934_SetTripAll(bool state);                         // 小计位置全显/全灭
void DRC_ET6934_SetClock(uint8_t hour, uint8_t minute);        // 设置时钟 (HH:MM)
// 5. 特定字符渲染 API
void DRC_ET6934_SetTrip10K_P(bool state);                      // 在 10K 位置显示或关闭字母 'P'

#endif // __DRC_ET6934_H
