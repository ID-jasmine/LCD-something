#ifndef __BSP_SYS_H
#define __BSP_SYS_H

#include "ddl.h"

void SysTick_Init(void);
void WDT_Init(void);
void delay100us_safe(uint32_t u32Cnt);

#endif
