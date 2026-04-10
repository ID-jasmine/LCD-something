#include "drv_adc.h"

#include "adc.h"
#include "bsp_adc.h"
#include "bsp_sys.h" // 需要调用 delay100us_safe


#define ADC_SAMPLES 10 // 滤波深度 (取10次平均)

// 初始化直接调 BSP 即可，不再需要 DMA 和 Timer
void DRV_ADC_Init(void) { BSP_ADC_Init(); }

// 一次性获取复数个通道的10次软件平均值
void DRV_Get_ADC_Avg(uint16_t *fuel_avg, uint16_t *power_avg, uint16_t *bat_volt_avg, uint16_t *hb_avg, uint16_t *p_gear_avg, uint16_t *start_stop_avg) {
  uint32_t f_sum = 0;
  uint32_t p_sum = 0;
  uint32_t b_sum = 0;
  uint32_t h_sum = 0;
  uint32_t pg_sum = 0;
  uint32_t ss_sum = 0;

  for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
    Adc_SQR_Start();
    delay100us_safe(1); // 等待转换完成

    // 从 SQR 结果寄存器中提取六个槽位的数据
    f_sum += M0P_ADC->SQRRESULT0; // 槽位0：油量
    p_sum += M0P_ADC->SQRRESULT1; // 槽位1：电源参考
    b_sum += M0P_ADC->SQRRESULT2; // 槽位2：电池电压
    h_sum += M0P_ADC->SQRRESULT3; // 槽位3：远光
    pg_sum += M0P_ADC->SQRRESULT4; // 槽位4：P档
    ss_sum += M0P_ADC->SQRRESULT5; // 槽位5：启停
  }

  *fuel_avg = (uint16_t)(f_sum / ADC_SAMPLES);
  *power_avg = (uint16_t)(p_sum / ADC_SAMPLES);
  *bat_volt_avg = (uint16_t)(b_sum / ADC_SAMPLES);
  *hb_avg = (uint16_t)(h_sum / ADC_SAMPLES);
  *p_gear_avg = (uint16_t)(pg_sum / ADC_SAMPLES);
  *start_stop_avg = (uint16_t)(ss_sum / ADC_SAMPLES);
}

// 深度休眠前：关闭模拟大户外设
void DRV_ADC_DeInit(void) {
    // 1. 停止 ADC 转换
    Adc_SQR_Stop();
    Adc_Disable();

    // 2. 彻底切断 ADC 模块的电源
    M0P_ADC->CR0 &= ~0x1u; 

    // 3. 彻底切断内部带隙基准电源 (BGR) —— 极其关键的省电步骤
    M0P_BGR->CR &= ~0x1u;  
}

