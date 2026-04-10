#ifndef __APP_UI_H
#define __APP_UI_H

#include "ddl.h"
#include "app_vehicle.h"

// 自检相关函数声明
void UI_SelfCheck(void);
void UI_UpdateNormalDisplay(void);

extern volatile uint8_t ZiJian_Start;
extern volatile uint8_t second;
extern volatile uint8_t minute;
extern volatile uint8_t hour;
extern volatile uint8_t is_running;

extern volatile uint8_t current_fault_show_idx;

#endif
