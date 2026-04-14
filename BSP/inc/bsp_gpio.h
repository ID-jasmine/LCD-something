#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

#include "ddl.h"

void BSP_GPIO_init(void);

void BSP_GPIO_CanInit(void);

void LPM_GPIO_Sleep_Config(void);

void LPM_GPIO_Wakeup_Config(void);

void BSP_GPIO_Unused_Init(void);

#endif
