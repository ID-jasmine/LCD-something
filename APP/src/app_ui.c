#include "app_ui.h"

#include "app_vehicle.h"
#include "bsp_can.h"
#include "drv_et6934.h"
#include "drv_touch.h"
#include "main.h"


volatile uint8_t ZiJian_Start = 0;
volatile uint8_t second = 0;
volatile uint8_t minute = 0;
volatile uint8_t hour = 0;
volatile uint8_t is_running = 0;
// 记录当前显示到第几个故障码 (0 代表 P1, 1 代表 P2 ...)
volatile uint8_t current_fault_show_idx = 0;

static void Render_FaultCode(uint16_t code);

// 自检函数
void UI_SelfCheck(void) {
    static uint32_t start_ms = 0;

    if (!is_running) {
        start_ms = Get_SystemMs();
        is_running = 1;
    }

    uint32_t now = Get_SystemMs();
    uint32_t elapsed = now - start_ms;

    if (elapsed < 3000) {
        DRC_ET6934_ClearAll();
        DRC_ET6934_SetCanvas(true);

        uint16_t anim_speed = 0;
        uint8_t anim_rpm = 0;
        uint8_t anim_fuel = 0;
        uint8_t anim_bat = 0;

        // 1. 车速逻辑：前1.5秒跳变上升，后1.5秒跳变回落
        if (elapsed <= 1500) {
            // 上升段：乘以 10 扩大区间，确保最后一个阶梯有足够的展示时间
            uint8_t step = (elapsed * 10) / 1500;
            if (step > 9)
                step = 9; // 强制封顶在第 9 阶 (即199)
            anim_speed = 100 + step * 11;
        } else {
            // 回落段：同理
            uint32_t rem = 3000 - elapsed;
            uint8_t step = (rem * 10) / 1500;
            if (step > 9)
                step = 9;
            anim_speed = 100 + step * 11;
        }

        // 2. 转速、油量、电压逻辑：用 2.8 秒的时间涨到满格，最后 0.2
        // 秒死死锁定满格，绝不闪烁！ 注意：转速的满格入参必须是
        // 39，这是你底层驱动的安全边界！
        anim_rpm = (elapsed * 39) / 2800;
        anim_fuel = (elapsed * 5) / 2800;
        anim_bat = (elapsed * 5) / 2800;

        // 边界防溢出保护，防止超过最大格数
        if (anim_rpm > 39)
            anim_rpm = 39;
        if (anim_fuel > 5)
            anim_fuel = 5;
        if (anim_bat > 5)
            anim_bat = 5;

        // 下发渲染参数
        DRC_ET6934_SetSpeed(anim_speed);
        DRC_ET6934_SetRPM(anim_rpm);
        DRC_ET6934_SetFuel(anim_fuel);
        DRC_ET6934_SetBattery(anim_bat);
        DRC_ET6934_SetTripAll(true);

        // 指示灯全亮
        DRC_ET6934_SetIndicator(IND_ABS, true);
        DRC_ET6934_SetIndicator(IND_TCS, true);
        DRC_ET6934_SetIndicator(IND_WATER_TEMP, true);
        DRC_ET6934_SetIndicator(IND_HIGH_BEAM, true);
        DRC_ET6934_SetIndicator(IND_SPEED_KMH, true);
        DRC_ET6934_SetIndicator(IND_ODO_MODE, true);
        DRC_ET6934_SetIndicator(IND_TRIP_MODE, true);
        DRC_ET6934_SetIndicator(IND_CLOCK_MODE, true);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, true);
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, true);
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, true);
        DRC_ET6934_SetIndicator(IND_TURN_RIGHT, true);
        DRC_ET6934_SetIndicator(IND_TURN_LEFT, true);
        DRC_ET6934_SetIndicator(IND_ENGINE_FAULT, true);
        DRC_ET6934_SetIndicator(IND_P_GEAR, true);
        DRC_ET6934_SetIndicator(IND_FAULT, true);
        DRC_ET6934_SetIndicator(IND_TRIP_UNIT_MILES, true);
        DRC_ET6934_SetIndicator(IND_TRIP_UNIT_KM, true);
        DRC_ET6934_SetIndicator(IND_SPEED_MPH, true);
        DRC_ET6934_SetIndicator(IND_START_STOP, true);

        DRC_ET6934_Refresh();
    } else {
        DRC_ET6934_ClearAll();
        DRC_ET6934_SetCanvas(true);
        DRC_ET6934_SetSpeed(0);
        DRC_ET6934_SetIndicator(IND_SPEED_KMH, true);
        Load_Mileage_From_EEPROM();
        DRV_Touch_GetEvent();
        ZiJian_Start = 1;
        is_running = 0;
        DRC_ET6934_Refresh();
    }
}

// 当你的按键检测到底层短按事件时，直接调用这个函数即可
void UI_NextFaultCode(void) {
    if (can_fault_count > 0) {
        Render_FaultCode(can_fault_codes[current_fault_show_idx]);

        current_fault_show_idx++;
        // 如果翻到最后一个，再按就回到 P1 (index 0)
        if (current_fault_show_idx >= can_fault_count) {
            current_fault_show_idx = 0;
        }
    }
}

// 渲染总里程
static void Render_ODO(uint32_t odo) {
    DRC_ET6934_SetIndicator(IND_ODO_MODE, true);

    uint32_t display_odo = odo;
    if (current_unit == UNIT_IMPERIAL) {
        display_odo = (uint32_t)((float)odo * 0.621371f + 0.5f);
        DRC_ET6934_SetIndicator(IND_TRIP_UNIT_MILES, true);
    } else {
        DRC_ET6934_SetIndicator(IND_TRIP_UNIT_KM, true);
    }

    DRC_ET6934_SetTrip(display_odo);
    // 确保总里程模式下，冒号（小数点）完全关闭
    DRC_ET6934_SetIndicator(IND_SEC_JUMP_0, false);
    DRC_ET6934_SetIndicator(IND_SEC_JUMP_1, false);
}

// 渲染小计里程
static void Render_TRIP(uint16_t trip) {
    DRC_ET6934_SetIndicator(IND_TRIP_MODE, true);

    uint16_t display_trip = trip;
    if (current_unit == UNIT_IMPERIAL) {
        // 1 km = 0.621371 miles
        display_trip = (uint16_t)((float)trip * 0.621371f + 0.5f);
        DRC_ET6934_SetIndicator(IND_TRIP_UNIT_MILES, true);
    } else {
        DRC_ET6934_SetIndicator(IND_TRIP_UNIT_KM, true);
    }

    DRC_ET6934_SetTripAll(false); // 先清空 6 位段码

    uint8_t d[4];
    d[0] = display_trip % 10;          // 0.1km位
    d[1] = (display_trip / 10) % 10;   // 个位
    d[2] = (display_trip / 100) % 10;  // 十位
    d[3] = (display_trip / 1000) % 10; // 百位

    // 把 0.1km 位放在冒号右侧(index 2)，个位放在冒号左侧(index 3)
    DRC_ET6934_SetSingleDigit(2, d[0]);
    DRC_ET6934_SetSingleDigit(3, d[1]);

    // 十位和百位按需点亮
    if (display_trip >= 100)
        DRC_ET6934_SetSingleDigit(4, d[2]);
    if (display_trip >= 1000)
        DRC_ET6934_SetSingleDigit(5, d[3]);

    // 点亮冒号的下半部分作为小数点，上半部分关闭
    DRC_ET6934_SetIndicator(IND_SEC_JUMP_0, false);
    DRC_ET6934_SetIndicator(IND_SEC_JUMP_1, true);
}

// 渲染故障码 (复用小计里程的位置)
static void Render_FaultCode(uint16_t code) {
    DRC_ET6934_SetTripAll(false); // 先清空 6 位段码

    uint8_t d[3];
    d[0] = code & 0x0F; // 最后一位
    d[1] = (code >> 4) & 0x0F;
    d[2] = (code >> 8) & 0x0F;

    // 把故障码放在小计里程的数码管上 (index 0~2)
    DRC_ET6934_SetSingleDigit(0, d[0]);
    DRC_ET6934_SetSingleDigit(1, d[1]);
    DRC_ET6934_SetSingleDigit(2, d[2]);

    // 显示 P + 序号 (例如 P1)
    DRC_ET6934_SetSingleDigit(5, current_fault_show_idx + 1);
    DRC_ET6934_SetTrip10K_P(true);

    // 关闭里程相关的单位和冒号，明确表示这是故障码而不是里程
    DRC_ET6934_SetIndicator(IND_SEC_JUMP_0, false);
    DRC_ET6934_SetIndicator(IND_SEC_JUMP_1, false);
    DRC_ET6934_SetIndicator(IND_TRIP_UNIT_KM, false);
    DRC_ET6934_SetIndicator(IND_TRIP_UNIT_MILES, false);
    DRC_ET6934_SetIndicator(IND_ODO_MODE, false);
    DRC_ET6934_SetIndicator(IND_TRIP_MODE, false);
}

// 渲染油量状态 (保持不变)
static void Render_Fuel(FuelState_t state) {
    bool blink_500ms = ((Get_SystemMs() / 500) % 2 == 0);
    switch (state) {
    case FUEL_STATE_ERR_FLASH:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, true);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, false);
        DRC_ET6934_SetFuel(blink_500ms ? 5 : 0);
        break;
    case FUEL_STATE_1_FLASH:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, true);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, false);
        DRC_ET6934_SetFuel(blink_500ms ? 1 : 0);
        break;
    case FUEL_STATE_1_SOLID:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, false);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, true);
        DRC_ET6934_SetFuel(1);
        break;
    case FUEL_STATE_2_SOLID:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, false);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, true);
        DRC_ET6934_SetFuel(2);
        break;
    case FUEL_STATE_3_SOLID:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, false);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, true);
        DRC_ET6934_SetFuel(3);
        break;
    case FUEL_STATE_4_SOLID:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, false);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, true);
        DRC_ET6934_SetFuel(4);
        break;
    case FUEL_STATE_5_SOLID:
        DRC_ET6934_SetIndicator(IND_FUEL_YELLOW, false);
        DRC_ET6934_SetIndicator(IND_FUEL_WHITE, true);
        DRC_ET6934_SetFuel(5);
        break;
    default:
        break;
    }
}

// 渲染电量状态 (保持不变)
static void Render_Battery(BatteryState_t state) {
    bool blink_500ms = ((Get_SystemMs() / 500) % 2 == 0);
    switch (state) {
    case BAT_STATE_1_FLASH:
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, blink_500ms);
        DRC_ET6934_SetBattery(blink_500ms ? 1 : 0);
        break;
    case BAT_STATE_1_SOLID:
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, false);
        DRC_ET6934_SetBattery(1);
        break;
    case BAT_STATE_2_SOLID:
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, false);
        DRC_ET6934_SetBattery(2);
        break;
    case BAT_STATE_3_SOLID:
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, false);
        DRC_ET6934_SetBattery(3);
        break;
    case BAT_STATE_4_SOLID:
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, false);
        DRC_ET6934_SetBattery(4);
        break;
    case BAT_STATE_5_SOLID:
        DRC_ET6934_SetIndicator(IND_BATTERY_ALARM, false);
        DRC_ET6934_SetBattery(5);
        break;
    default:
        break;
    }
}

// 正常显示刷新任务
void UI_UpdateNormalDisplay(void) {
    DRC_ET6934_ClearAll();
    DRC_ET6934_SetCanvas(true);

    // 1. 渲染基础元素
    DRC_ET6934_SetSpeed(SpeedDriver);
    if (current_unit == UNIT_IMPERIAL) {
        DRC_ET6934_SetIndicator(IND_SPEED_MPH, true);
    } else {
        DRC_ET6934_SetIndicator(IND_SPEED_KMH, true);
    }

    uint8_t rpm_level = (uint32_t)rpmDriver * 40 / 10000;
    if (rpm_level > 39)
        rpm_level = 39;
    DRC_ET6934_SetRPM(rpm_level);

    Render_Fuel(current_fuel_state);
    Render_Battery(current_bat_state);

    DRC_ET6934_SetIndicator(IND_HIGH_BEAM, is_high_beam_on);
    DRC_ET6934_SetIndicator(IND_P_GEAR, is_p_gear_on);
    DRC_ET6934_SetIndicator(IND_START_STOP, is_position_light_on);
    DRC_ET6934_SetIndicator(IND_ABS, is_ABS_on);
    DRC_ET6934_SetIndicator(IND_TCS, is_TCS_on);

    bool blink_500ms = ((Get_SystemMs() / 500) % 2 == 0);

    // 水温灯控制逻辑
    if (engine_water_temp >= 125.0f) {
        DRC_ET6934_SetIndicator(IND_WATER_TEMP, true); // 常显
    } else if (engine_water_temp >= 65.0f) {
        DRC_ET6934_SetIndicator(IND_WATER_TEMP, blink_500ms); // 闪烁
    } else {
        DRC_ET6934_SetIndicator(IND_WATER_TEMP, false); // 不亮
    }
    // 引擎故障灯亮灭受数量控制
    DRC_ET6934_SetIndicator(IND_ENGINE_FAULT, (can_fault_count > 0));

    // 2. 渲染里程/时钟区域
    if (can_fault_count > 0) {
        Render_FaultCode(can_fault_codes[current_fault_show_idx]);
    } else {
        switch (current_display_mode) {
        case MODE_ODO:
            Render_ODO(ODO_Value);
            break;
        case MODE_TRIP:
            Render_TRIP(TRIP_Value);
            break;
        case MODE_CLOCK:
            DRC_ET6934_SetClock(hour, minute);
            DRC_ET6934_SetIndicator(IND_CLOCK_MODE, true);
            DRC_ET6934_SetIndicator(IND_SEC_JUMP_0, blink_500ms);
            DRC_ET6934_SetIndicator(IND_SEC_JUMP_1, blink_500ms);
            break;
        case MODE_TIME_SET_HOUR:
            DRC_ET6934_SetTripAll(false); // 全局清空数字区域

            // 分钟常亮显示 (index 1 和 2)
            DRC_ET6934_SetSingleDigit(1, minute % 10);
            DRC_ET6934_SetSingleDigit(2, minute / 10);

            // 小时按周期闪烁 (index 3 和 4)
            if (blink_500ms) {
                DRC_ET6934_SetSingleDigit(3, hour % 10);
                if (hour / 10 > 0)
                    DRC_ET6934_SetSingleDigit(4, hour / 10);
            }

            DRC_ET6934_SetIndicator(IND_CLOCK_MODE, true);
            DRC_ET6934_SetIndicator(IND_SEC_JUMP_0, true); // 设置时间时冒号常亮
            DRC_ET6934_SetIndicator(IND_SEC_JUMP_1, true);
            break;
        case MODE_TIME_SET_MIN:
            DRC_ET6934_SetTripAll(false); // 全局清空数字区域

            // 小时常亮显示
            DRC_ET6934_SetSingleDigit(3, hour % 10);
            if (hour / 10 > 0)
                DRC_ET6934_SetSingleDigit(4, hour / 10);

            // 分钟按周期闪烁
            if (blink_500ms) {
                DRC_ET6934_SetSingleDigit(1, minute % 10);
                DRC_ET6934_SetSingleDigit(2, minute / 10);
            }

            DRC_ET6934_SetIndicator(IND_CLOCK_MODE, true);
            DRC_ET6934_SetIndicator(IND_SEC_JUMP_0, true);
            DRC_ET6934_SetIndicator(IND_SEC_JUMP_1, true);
            break;
        }
    }

    DRC_ET6934_Refresh();
}
