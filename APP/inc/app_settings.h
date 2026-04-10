#ifndef __APP_SETTINGS_H
#define __APP_SETTINGS_H

#include "ddl.h"

// 系统参数定义
extern uint16_t sys_tire_perimeter; // 轮胎周长 (1000~2999)
extern uint8_t  sys_speed_pulse;    // 每圈脉冲数 (1~50)

void UI_Button_Task(void);

#endif
