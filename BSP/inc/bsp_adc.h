#ifndef __BSP_ADC_H__
#define __BSP_ADC_H__

#include "ddl.h" // 包含华大标准库基础头文件

// 初始化 ADC 硬件、GPIO 及相关模式
void BSP_ADC_Init(void);

// 启动 ADC 扫描（如果有需要手动干预的场景预留）
void BSP_ADC_Start(void);

// 停止 ADC 扫描
void BSP_ADC_Stop(void);

void DRV_ADC_DeInit(void);
void DRV_ADC_Wakeup(void);

#endif /* __BSP_ADC_H__ */
