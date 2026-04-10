#include "app_vehicle.h"

#include "main.h"

#include "app_settings.h"
#include "drv_adc.h"
#include "drv_eeprom.h"
#include "drv_et6934.h"


#include "gpio.h"

uint16_t speed_pulse_cnt = 0;
uint16_t rpm_pulse_cnt = 0;
uint16_t SpeedDriver = 0;
uint16_t rpmDriver = 0;
uint32_t ODO_Value = 0;  // 单位: km
uint16_t TRIP_Value = 0; // 单位: 0.1km
UnitSystem_t current_unit = UNIT_METRIC;
DisplayMode_t current_display_mode = MODE_ODO;

FuelState_t current_fuel_state =
    FUEL_STATE_5_SOLID; // 对外输出的当前稳定油量状态
BatteryState_t current_bat_state = BAT_STATE_5_SOLID; // 默认满电

static uint16_t speedDriver_inn = 0;
static uint16_t rpmDriver_inn = 0;
bool is_high_beam_on = false;
bool is_p_gear_on = false;
bool is_position_light_on = false;
bool is_ABS_on = false;
bool is_TCS_on = false;
bool is_first_sensor_read = true;

// 内部累计脉冲
static uint32_t total_pulses_for_trip = 0;
static uint32_t total_pulses_for_odo = 0;

static uint8_t eeprom_seq = 0; // 0~255 自动溢出循环

static uint16_t Get_Indicated_Speed(uint16_t actual_speed);
static void Process_ADC_Indicator(ADC_Indicator_t *indicator, uint16_t adc_val);

// 实例化你的 3 个灯光通道，将参数直接填进去
static ADC_Indicator_t ind_high_beam = {V_DIVIDER, 7.5f, 7.0f,
                                        2,         0,    &is_high_beam_on};
static ADC_Indicator_t ind_p_gear = {P_GEAR_DIVIDER, 7.5f, 7.0f, 2, 0,
                                     &is_p_gear_on};
static ADC_Indicator_t ind_pos_light = {V_DIVIDER, 7.5f, 7.0f,
                                        2,         0,    &is_position_light_on};

// 车速 建议 100ms 调用一次
uint16_t SpeedDriver_Process(void) {
    uint16_t speedDriver = 0;
    uint32_t calc_speed = 0;

    static uint16_t last_speed_pulse_cnt =
        0; // 引入静态变量记录上一次的总脉冲数

    // 1. 安全读取但不清零
    __disable_irq();
    uint16_t current_total_pulse = speed_pulse_cnt;
    __enable_irq();

    // 利用无符号整型溢出特性，无损计算 100ms 内的增量
    uint16_t current_pulse = current_total_pulse - last_speed_pulse_cnt;
    last_speed_pulse_cnt = current_total_pulse;

    // --- 里程累加逻辑 (由于 current_pulse
    // 还是表示这100ms的脉冲，以下完全无需修改) ---
    // 2800 脉冲 = 1 km, 280 脉冲 = 0.1 km
    total_pulses_for_trip += current_pulse;
    total_pulses_for_odo += current_pulse;

    // 处理小计里程 (0.1km 精度)
    if (total_pulses_for_trip >= 280) {
        uint16_t increment_01km = total_pulses_for_trip / 280;
        total_pulses_for_trip %= 280;

        TRIP_Value += increment_01km;
        if (TRIP_Value > TRIP_MAX_VALUE) {
            TRIP_Value = 0; // 超过 999.9 自动清零
        }

        // 每 0.1km 存储一次ODO
        Save_Mileage_To_EEPROM();
    }

    // 处理总里程 (1km 精度)
    if (total_pulses_for_odo >= 2800) {
        uint32_t increment_1km = total_pulses_for_odo / 2800;
        total_pulses_for_odo %= 2800;

        if (ODO_Value < ODO_MAX_VALUE) {
            ODO_Value += increment_1km;
            if (ODO_Value > ODO_MAX_VALUE)
                ODO_Value = ODO_MAX_VALUE;
        }
    }

    // --- 车速计算逻辑 ---
    static uint16_t pulse_buf[10] = {0};
    static uint8_t buf_index = 0;
    uint32_t sum_pulse = 0;

    pulse_buf[buf_index] = current_pulse;
    buf_index++;
    if (buf_index >= 10)
        buf_index = 0;

    for (uint8_t i = 0; i < 10; i++)
        sum_pulse += pulse_buf[i];

    uint32_t denominator = (uint32_t)sys_speed_pulse * 2500UL;
    if (denominator > 0) {
        uint32_t round_offset = denominator / 2;
        calc_speed =
            (sum_pulse * (uint32_t)sys_tire_perimeter * 9UL + round_offset) /
            denominator;
    } else {
        calc_speed = 0;
    }

    if (calc_speed < 2) {
        speedDriver = 0;
    } else {
        speedDriver = Get_Indicated_Speed((uint16_t)calc_speed);
        if (speedDriver > 199)
            speedDriver = 199;
    }

    speedDriver_inn = speedDriver; // 内部逻辑（如油量延迟）统一用公制

    // 如果是英制，进行转换 (1 km = 0.621371 miles)
    if (current_unit == UNIT_IMPERIAL) {
        speedDriver = (uint16_t)((float)speedDriver * 0.621371f + 0.5f);
    }

    return speedDriver;
}

static uint16_t Get_Indicated_Speed(uint16_t actual_speed) {
#define SPEED_MAP_SIZE 9
    const uint16_t Map_X[SPEED_MAP_SIZE] = {0,  10,  20,  40, 60,
                                            80, 100, 120, 140};
    const uint16_t Map_Y[SPEED_MAP_SIZE] = {0,  10,  21,  42, 63,
                                            84, 106, 126, 150};

    if (actual_speed >= 140) {
        uint16_t dx = 140 - 120;
        uint16_t dy = 150 - 126;
        return 150 + ((actual_speed - 140) * dy + dx / 2) / dx;
    }

    for (uint8_t i = 0; i < SPEED_MAP_SIZE - 1; i++) {
        if (actual_speed >= Map_X[i] && actual_speed < Map_X[i + 1]) {
            uint16_t x0 = Map_X[i];
            uint16_t x1 = Map_X[i + 1];
            uint16_t y0 = Map_Y[i];
            uint16_t y1 = Map_Y[i + 1];
            uint16_t dx = x1 - x0;
            uint16_t dy = y1 - y0;
            return y0 + ((actual_speed - x0) * dy + dx / 2) / dx;
        }
    }
    return actual_speed;
}

// 转速
uint16_t RpmDriver_Process(void) {
    uint16_t rpmDriver = 0;

    // 差值法，引入静态变量记录上一次的总脉冲数
    static uint16_t last_rpm_pulse_cnt = 0;

    // 1. 安全读取但不清零
    __disable_irq();
    uint16_t current_total_pulse = rpm_pulse_cnt;
    __enable_irq();

    // 差值法计算这 100ms 内新增的脉冲
    uint16_t current_pulse = current_total_pulse - last_rpm_pulse_cnt;
    last_rpm_pulse_cnt = current_total_pulse;

    // 2. 500ms 滑动窗口缓冲 (100ms * 5 = 0.5s)
    static uint16_t rpm_pulse_buf[5] = {0};
    static uint8_t rpm_buf_idx = 0;
    uint32_t sum_rpm_pulse = 0;

    rpm_pulse_buf[rpm_buf_idx] = current_pulse;
    rpm_buf_idx++;
    if (rpm_buf_idx >= 5) {
        rpm_buf_idx = 0;
    }

    // 累加得到 0.5 秒内的总脉冲数
    for (uint8_t i = 0; i < 5; i++) {
        sum_rpm_pulse += rpm_pulse_buf[i];
    }

    // 3. 计算基础转速
    // 公式：脉冲数/0.5秒 -> 乘以2得到 Hz -> 乘以60得到 RPM
    // 简化合并：sum_rpm_pulse * 120
    uint32_t calc_rpm = sum_rpm_pulse * 120;

    // 限制最大值为 10000 rpm，防止超出指示范围
    if (calc_rpm > 10000) {
        calc_rpm = 10000;
    }

    // 4. IIR 迟滞滤波算法
    static uint16_t filtered_rpm = 0;

    if (calc_rpm > filtered_rpm) {
        // 踩油门（上升期）：75% 权重给新采集的转速，响应极快跟脚
        filtered_rpm = (filtered_rpm * 1 + calc_rpm * 3) / 4;
    } else {
        // 松油门（下降或稳态）：50% 权重，加快回落，消除微小抖动
        filtered_rpm = (filtered_rpm * 1 + calc_rpm * 1) / 2;
    }

    /* * 关于响应时间的验证：
     * - 窗口填满需要 500ms
     * - IIR 滤波达到 95% 的目标值大约需要 2~3 个周期 (200~300ms)
     * - 总响应时间约为 0.7~0.8 秒，完美符合国标 0.5~1.5 秒的要求
     */

    // 5. 输出处理
    rpmDriver = filtered_rpm;

    rpmDriver_inn = rpmDriver;
    return rpmDriver;
}

// fuel和电压 (每100ms被main循环调用一次)
void vehicle_sensor_process(void) {
    uint16_t fuel_adc_val = 0;
    uint16_t power_adc_val = 0;
    uint16_t bat_volt_adc_val = 0; // 新增：电池电压 ADC 原始值
    uint16_t hb_adc_val = 0;       // 新增：远光 ADC 原始值
    uint16_t p_adc_val = 0;        // 新增：P档 ADC 原始值
    uint16_t w_adc_val = 0;        // 新增：启停灯 ADC 原始值

    float ratio = 0.0f;
    float r_sensor = 0.0f;
    FuelState_t target_fuel_state = FUEL_STATE_ERR_FLASH;
    static uint16_t fuel_delay_cnt = 0; // 步进迟滞滤波计数器

    // 获取平滑后的 ADC 平均值 (PB13 为油量，PB14
    // 为电源参考，PC5为电池，PB1为远光, PB2为P档)
    DRV_Get_ADC_Avg(&fuel_adc_val, &power_adc_val, &bat_volt_adc_val,
                    &hb_adc_val, &p_adc_val, &w_adc_val);

    // 通用通道处理 (远光、P档、位置灯)
    // 只需要把对应的结构体地址和读取到的 ADC
    // 值传进去，它就会自动帮你算好并更新全局变量！
    Process_ADC_Indicator(&ind_high_beam, hb_adc_val);
    Process_ADC_Indicator(&ind_p_gear, p_adc_val);
    Process_ADC_Indicator(&ind_pos_light, w_adc_val);

    if (!Gpio_GetInputIO(GpioPortC, GpioPin11))
        is_ABS_on = true;
    else
        is_ABS_on = false;

    if (!Gpio_GetInputIO(GpioPortC, GpioPin12))
        is_TCS_on = true;
    else
        is_TCS_on = false;

    // 油量处理逻辑
    if (power_adc_val != 0) {
        ratio = (float)fuel_adc_val / (float)power_adc_val;
        if (ratio < 0.99f) {
            r_sensor = FUEL_PULLUP_RES * (ratio / (1.0f - ratio));

            if (r_sensor <= 13.0f) {
                target_fuel_state = FUEL_STATE_5_SOLID;
            } else if (r_sensor <= 35.0f) {
                target_fuel_state = FUEL_STATE_4_SOLID;
            } else if (r_sensor <= 58.0f) {
                target_fuel_state = FUEL_STATE_3_SOLID;
            } else if (r_sensor <= 75.0f) {
                target_fuel_state = FUEL_STATE_2_SOLID;
            } else if (r_sensor <= 94.0f) {
                target_fuel_state = FUEL_STATE_1_SOLID;
            } else if (r_sensor <= 180.0f) {
                target_fuel_state = FUEL_STATE_1_FLASH;
            } else {
                target_fuel_state = FUEL_STATE_ERR_FLASH;
            }
        }
    }

    // 电池电压处理逻辑 (带迟滞与延时滤波)
    BatteryState_t target_bat_state = BAT_STATE_5_SOLID;
    static uint16_t bat_delay_cnt = 0;

    float node_voltage = ((float)bat_volt_adc_val * VREF / 4095.0f) * V_DIVIDER;
    // 加上二极管压降，得到真实的 IGN(+) 电池输入电压
    float voltage = node_voltage + V_DIODE;

    // 状态映射 (包含要求的电压范围)
    if (voltage <= 11.5f) {
        target_bat_state = BAT_STATE_1_FLASH;
    } else if (voltage <= 12.0f) {
        target_bat_state = BAT_STATE_1_SOLID;
    } else if (voltage <= 12.5f) {
        target_bat_state = BAT_STATE_2_SOLID;
    } else if (voltage <= 13.5f) {
        target_bat_state = BAT_STATE_3_SOLID;
    } else if (voltage <= 14.0f) {
        target_bat_state = BAT_STATE_4_SOLID;
    } else {
        target_bat_state = BAT_STATE_5_SOLID;
    }

    if (is_first_sensor_read) {
        // 刚上电自检完毕的第一帧：无视任何延迟，直接显示当前真实状态！
        current_fuel_state = target_fuel_state;
        current_bat_state = target_bat_state;

        is_first_sensor_read = false; // 触发后立刻清除标志，恢复后续的阻尼逻辑
        fuel_delay_cnt = 0;
        bat_delay_cnt = 0;
    } else {
        // --- 原有的油量步进延迟逻辑 ---
        if (target_fuel_state != current_fuel_state) {
            fuel_delay_cnt++;
            uint16_t threshold_cnt =
                ((speedDriver_inn > 0) || (rpmDriver_inn > 0)) ? 550 : 50;
            if (fuel_delay_cnt >= threshold_cnt) {
                if (target_fuel_state > current_fuel_state)
                    current_fuel_state++;
                else
                    current_fuel_state--;
                fuel_delay_cnt = 0;
            }
        } else {
            fuel_delay_cnt = 0;
        }

        // 步进延时滤波 (滤除 ±0.3V 造成的抖动，这里设定为 20 * 100ms = 2秒)
        if (target_bat_state != current_bat_state) {
            bat_delay_cnt++;
            if (bat_delay_cnt >= 20) { // 连续 2 秒维持新状态才切换
                if (target_bat_state > current_bat_state)
                    current_bat_state++;
                else
                    current_bat_state--;
                bat_delay_cnt = 0;
            }
        } else {
            bat_delay_cnt = 0;
        }
    }
}

// 通用 ADC 指示灯处理核心逻辑
static void Process_ADC_Indicator(ADC_Indicator_t *indicator,
                                  uint16_t adc_val) {
    // 1. 根据结构体自带的分压比，计算真实电压
    float voltage =
        ((float)adc_val * VREF / 4095.0f) * indicator->divider + V_DIODE;

    // 2. 获取当前状态
    bool target_state = *(indicator->out_state);

    // 3. 迟滞电压判断
    if (voltage > indicator->threshold_on) {
        target_state = true;
    } else if (voltage < indicator->threshold_off) {
        target_state = false;
    }

    // 4. 时间消抖滤波
    if (target_state != *(indicator->out_state)) {
        indicator->delay_cnt++;
        if (indicator->delay_cnt >= indicator->delay_max) {
            *(indicator->out_state) = target_state; // 直接更新全局变量
            indicator->delay_cnt = 0;
        }
    } else {
        indicator->delay_cnt = 0;
    }
}

// EEPROM 存储逻辑
// 异或校验函数，保护前 len 个字节
static uint8_t calc_checksum(uint8_t *buf, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum ^= buf[i];
    }
    return sum;
}

void Save_Mileage_To_EEPROM(void) {
    uint8_t buffer[8];
    uint8_t flags =
        ((uint8_t)current_unit << 4) | ((uint8_t)current_display_mode & 0x0F);

    eeprom_seq++; // 序列号递增，超 255 自动变 0

    buffer[0] = (uint8_t)(ODO_Value >> 24);
    buffer[1] = (uint8_t)(ODO_Value >> 16);
    buffer[2] = (uint8_t)(ODO_Value >> 8);
    buffer[3] = (uint8_t)ODO_Value;
    buffer[4] = eeprom_seq;               // 存入序列号
    buffer[5] = flags;                    // 存入状态标志
    buffer[6] = calc_checksum(buffer, 6); // 校验前 6 个字节
    buffer[7] = 0x5A;                     // 填充字节 (固定魔术字)

    // 奇偶交替写入：偶数存 A 区，奇数存 B 区
    if (eeprom_seq % 2 == 0) {
        (void)EEPROM_Device_WriteBuffer(&g_eeprom_dev, ADDR_SLOT_A, buffer, 8);
    } else {
        (void)EEPROM_Device_WriteBuffer(&g_eeprom_dev, ADDR_SLOT_B, buffer, 8);
    }
}
void Load_Mileage_From_EEPROM(void) {
    uint8_t bufA[8], bufB[8];
    uint8_t validA = 0, validB = 0;
    uint8_t retry = 3; // 新增：重试3次机制，专治EEPROM上电起步慢

    while (retry--) {
        validA = 0;
        validB = 0;
        (void)EEPROM_Device_ReadBuffer(&g_eeprom_dev, ADDR_SLOT_A, bufA, 8);
        (void)EEPROM_Device_ReadBuffer(&g_eeprom_dev, ADDR_SLOT_B, bufB, 8);

        if (bufA[6] == calc_checksum(bufA, 6) && bufA[7] == 0x5A)
            validA = 1;
        if (bufB[6] == calc_checksum(bufB, 6) && bufB[7] == 0x5A)
            validB = 1;

        if (validA || validB)
            break; // 只要有一个能读出来，立刻跳出重试

        // 如果都读不出来，千万别急着清零，等 30ms 再试一次！
        uint32_t start = Get_SystemMs();
        while (Get_SystemMs() - start < 30)
            ;
    }

    uint8_t *best_buf = NULL;

    // 2. 稳健决策逻辑
    if (validA && validB) {
        int8_t diff = (int8_t)(bufA[4] - bufB[4]);
        if (diff > 0)
            best_buf = bufA;
        else
            best_buf = bufB;
    } else if (validA) {
        best_buf = bufA;
    } else if (validB) {
        best_buf = bufB;
    }

    // 3. 解析数据并应用
    if (best_buf != NULL) {
        ODO_Value = ((uint32_t)best_buf[0] << 24) |
                    ((uint32_t)best_buf[1] << 16) |
                    ((uint32_t)best_buf[2] << 8) | best_buf[3];
        eeprom_seq = best_buf[4];

        uint8_t flags = best_buf[5];
        current_unit = (UnitSystem_t)(flags >> 4);
        current_display_mode = (DisplayMode_t)(flags & 0x0F);

        if (current_display_mode > MODE_CLOCK)
            current_display_mode = MODE_CLOCK;
    } else {
        // 只有重试 3 次依然全错，才判定为新板子或全损
        ODO_Value = 0;
        eeprom_seq = 0;
        current_unit = UNIT_METRIC;
        current_display_mode = MODE_ODO;
    }

    // 把之前的 debug 代码删掉，恢复成 0 即可
    TRIP_Value = 0;
}
