#ifndef __DRV_ADC_H
#define __DRV_ADC_H

#include "ddl.h"

// ==========================================================
// DRV 接口暴露
// ==========================================================
// 驱动层初始化 (包含 ADC, DMA, Timer的级联启动)
void DRV_ADC_Init(void);

// 获取 ADC 值
void DRV_Get_ADC_Avg(uint16_t *fuel_avg, uint16_t *power_avg, uint16_t *bat_volt_avg, uint16_t *hb_avg, uint16_t *p_gear_avg, uint16_t *start_stop_avg);

void DRV_ADC_DeInit(void);
void DRV_ADC_Wakeup(void);

#endif /* __DRV_ADC_H */
