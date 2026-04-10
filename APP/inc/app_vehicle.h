#ifndef __APP_VEHICLE_H
#define __APP_VEHICLE_H

#include "ddl.h"
#include <stdint.h>

#define ODO_MAX_VALUE      999999 // ODO最大显示 (km)
#define TRIP_MAX_VALUE     9999   // TRIP最大显示 (0.1km为单位，即999.9 km)

// 磨损均衡法参数
#define SLOT_SIZE		8       // 每个槽位 8 字节
#define SLOT_COUNT		10      // 10个槽位轮流写
#define ADDR_SLOT_A     0x00    // 主区起始地址  #define ADDR_SLOT_A 0x00
#define ADDR_SLOT_B		0x10    // 备份区起始地址 #define ADDR_SLOT_B 0x08 
typedef enum {
    UNIT_METRIC = 0,   // 公制 (km/h, km)
    UNIT_IMPERIAL      // 英制 (mph, miles)
} UnitSystem_t;

typedef enum {
    MODE_ODO = 0,
    MODE_TRIP,
    MODE_CLOCK,
    MODE_TIME_SET_HOUR,
    MODE_TIME_SET_MIN
} DisplayMode_t;

extern bool is_first_sensor_read;
extern UnitSystem_t current_unit;
extern DisplayMode_t current_display_mode;
extern uint16_t SpeedDriver; // 改为外部引用
extern uint16_t rpmDriver;
// ================= 新增 CAN 信号全局变量 =================
extern volatile float engine_water_temp;       // 发动机水温

// 燃油表分压上拉电阻（理论200欧，这里保留你之前的等效修正值174，可根据新板子实测微调）
#define FUEL_PULLUP_RES 174.0f

// 远光和电压和启停硬件参数
#define VREF       5.0f   // 或者 3.3f，视硬件而定
#define V_DIVIDER  4.0f  
#define V_DIODE    0.7f   // 预估二极管压降，可根据实测微调

// --- P档专属硬件参数 ---
#define P_GEAR_DIVIDER   (67.0f / 24.0f)  // 精确计算，避免浮点数精度截断

// 定义一个通用的 ADC 指示灯控制结构体
typedef struct {
    float divider;          // 硬件分压比
    float threshold_on;     // 点亮阈值 (V)
    float threshold_off;    // 熄灭阈值 (V)
    uint8_t delay_max;      // 消抖周期 (x 100ms)
    uint8_t delay_cnt;      // 内部使用的消抖计数器 (初始化填 0 即可)
    bool* out_state;        // 指向外部全局状态变量的指针 (非常关键，直接修改全局变量)
} ADC_Indicator_t;

// 燃油表 7 种逻辑状态
typedef enum {
    FUEL_STATE_ERR_FLASH = 0,   // R > 180: 图标黄色，五格全闪
    FUEL_STATE_1_FLASH   = 1,   // 94 < R <= 180: 图标黄色，一格红色闪烁
    FUEL_STATE_1_SOLID   = 2,   // 75 < R <= 94: 图标白色，一格红色常显
    FUEL_STATE_2_SOLID   = 3,   // 58 < R <= 75: 图标白色，两格常显
    FUEL_STATE_3_SOLID   = 4,   // 35 < R <= 58: 图标白色，三格常显
    FUEL_STATE_4_SOLID   = 5,   // 13 < R <= 35: 图标白色，四格常显
    FUEL_STATE_5_SOLID   = 6    // R <= 13: 图标白色，五格常显
} FuelState_t;

extern FuelState_t current_fuel_state; // 供 app_ui.c 调用的燃油状态

// 电池显示状态枚举
typedef enum {
    BAT_STATE_1_FLASH = 0, // U <= 11.5
    BAT_STATE_1_SOLID,     // 11.5 < U <= 12.0
    BAT_STATE_2_SOLID,     // 12.0 < U <= 12.5
    BAT_STATE_3_SOLID,     // 12.5 < U <= 13.5
    BAT_STATE_4_SOLID,     // 13.5 < U <= 14.0
    BAT_STATE_5_SOLID      // U > 14.0
} BatteryState_t;

// 对外输出的当前稳定电池状态
extern BatteryState_t current_bat_state;

extern bool is_high_beam_on;
extern bool is_p_gear_on;
extern bool is_position_light_on;
extern bool is_ABS_on;
extern bool is_TCS_on;

extern uint16_t speed_pulse_cnt;
extern uint16_t rpm_pulse_cnt;
extern uint32_t ODO_Value;        // 总里程 (km)
extern uint16_t TRIP_Value;       // 小计里程 (0.1km单位)

uint16_t SpeedDriver_Process(void);
uint16_t RpmDriver_Process(void);
void vehicle_sensor_process(void);

void Load_Mileage_From_EEPROM(void);
void Save_Mileage_To_EEPROM(void);

#endif
