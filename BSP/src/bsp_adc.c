#include "bsp_adc.h"

#include "adc.h"
#include "bgr.h"
#include "bsp_sys.h"
#include "gpio.h"
#include "wdt.h"
#include <string.h>


// ==========================================================
// 硬件引脚映射宏定义 (方便后期修改)
// ==========================================================
#define FUEL_ADC_PORT GpioPortB
#define FUEL_ADC_PIN GpioPin13
#define FUEL_ADC_CH AdcExInputCH20 // PB13 标准对应 CH20

#define AD_POWER_ADC_PORT GpioPortB
#define AD_POWER_ADC_PIN GpioPin14
#define AD_POWER_ADC_CH AdcExInputCH21 // PB14 标准对应 CH21

#define BAT_VOLT_ADC_PORT GpioPortC
#define BAT_VOLT_ADC_PIN GpioPin5
#define BAT_VOLT_ADC_CH AdcExInputCH15 // PC05 标准对应 CH15

#define HIGH_BEAM_ADC_PORT GpioPortB
#define HIGH_BEAM_ADC_PIN GpioPin1
#define HIGH_BEAM_ADC_CH AdcExInputCH9 // PB1 标准对应 CH9

#define P_GEAR_ADC_PORT GpioPortB
#define P_GEAR_ADC_PIN GpioPin2
#define P_GEAR_ADC_CH AdcExInputCH16 // PB2 标准对应 CH16

#define START_STOP_ADC_PORT GpioPortB
#define START_STOP_ADC_PIN GpioPin12
#define START_STOP_ADC_CH AdcExInputCH19 // PB12 标准对应 CH19

// ==========================================================
// BSP 接口函数实现
// ==========================================================


void BSP_ADC_Init(void) {
  static stc_adc_cfg_t stcAdcCfg;
  static stc_adc_sqr_cfg_t stcAdcSqrCfg;

  // 将结构体内存全部清零，防止隐形字段填入乱码
  memset(&stcAdcCfg, 0, sizeof(stc_adc_cfg_t));
  memset(&stcAdcSqrCfg, 0, sizeof(stc_adc_sqr_cfg_t));

  // 1. 开启外设时钟 (需根据华大具体系列包含 sysctrl.h，此处参考 main.c 逻辑)
  Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
  Sysctrl_SetPeripheralGate(SysctrlPeripheralAdcBgr, TRUE);

  // 2. 配置引脚为模拟输入模式
  Gpio_SetAnalogMode(FUEL_ADC_PORT, FUEL_ADC_PIN);
  Gpio_SetAnalogMode(AD_POWER_ADC_PORT, AD_POWER_ADC_PIN);
  Gpio_SetAnalogMode(BAT_VOLT_ADC_PORT, BAT_VOLT_ADC_PIN);
  Gpio_SetAnalogMode(HIGH_BEAM_ADC_PORT, HIGH_BEAM_ADC_PIN);
  Gpio_SetAnalogMode(P_GEAR_ADC_PORT, P_GEAR_ADC_PIN);
  Gpio_SetAnalogMode(START_STOP_ADC_PORT, START_STOP_ADC_PIN);

  // 3. 开启内部带隙基准 (BGR)，这是保证华大 ADC 稳定工作的前提
  // Bgr_BgrEnable();
  M0P_BGR->CR |= 0x1u;
  delay100us_safe(1); // 替代 delay10us(2)，确保 BGR 稳定

  // 4. ADC 基础初始化
  //    stcAdcCfg.enAdcMode         = AdcScanMode;          //
  //    核心：设为多通道扫描模式 stcAdcCfg.enAdcClkDiv       = AdcMskClkDiv1; //
  //    采样分频，视你系统主频而定 stcAdcCfg.enAdcSampCycleSel =
  //    AdcMskSampCycle8Clk;  // 采样周期数，油量是慢信号，稍微设长一点没关系
  //    stcAdcCfg.enAdcRefVolSel    = AdcMskRefVolSelAVDD;  //
  //    核心：参考电压选择 AVDD (比例测量的物理前提!) stcAdcCfg.enAdcOpBuf =
  //    AdcMskBufDisable;     // 关闭内部 OP BUF stcAdcCfg.enInRef           =
  //    AdcMskInRefDisable;   // 关闭内部参考源 stcAdcCfg.enAdcAlign        =
  //    AdcAlignRight;        // 结果右对齐，方便直接读取数值
  //    Adc_Init(&stcAdcCfg);

  M0P_ADC->CR0 = 0x1u; // 开启 ADC 电源
  delay100us_safe(1);  // 等待 ADC 稳定

  // 配置时钟、参考电压和采样周期
  M0P_ADC->CR0 |= (uint32_t)AdcMskClkDiv1 | (uint32_t)AdcMskRefVolSelAVDD |
                  (uint32_t)AdcMskBufDisable | (uint32_t)AdcMskSampCycle8Clk |
                  (uint32_t)AdcMskInRefDisable;

  M0P_ADC->CR1_f.MODE = AdcScanMode;
  M0P_ADC->CR1_f.ALIGN = AdcAlignRight;

  // 5. 顺序扫描模式(SQR)配置
  stcAdcSqrCfg.u8SqrCnt = 6; // 关键：我们要扫描六个通道 (油量 + 电源 + 电池 + 远光 + P档 + 启停)
  stcAdcSqrCfg.enResultAcc = AdcResultAccDisable; // 关闭硬件自动累加，我们要用 DMA
                           // 搬运到内存，用纯软件算法做滑动平均
  // stcAdcSqrCfg.bSqrDmaTrig = TRUE;                // 关键：开启扫描完成触发
  // DMA！(这为咱们下一步配置 DMA 留好接口)
  stcAdcSqrCfg.bSqrDmaTrig = FALSE; // 硬件DMA无法成功，以后再说
  Adc_SqrModeCfg(&stcAdcSqrCfg);

  // 6. 配置扫描通道序列
  // 扫描槽位 0：采集油量 FUEL_ADC
  Adc_CfgSqrChannel(AdcSQRCH0MUX, FUEL_ADC_CH);
  // 扫描槽位 1：采集基准电源 AD_POWER_ADC
  Adc_CfgSqrChannel(AdcSQRCH1MUX, AD_POWER_ADC_CH);
  // 扫描槽位 2：采集电池电压 BAT_VOLT_ADC
  Adc_CfgSqrChannel(AdcSQRCH2MUX, BAT_VOLT_ADC_CH);
  // 扫描槽位 3：采集远光 HIGH_BEAM_ADC
  Adc_CfgSqrChannel(AdcSQRCH3MUX, HIGH_BEAM_ADC_CH);
  // 扫描槽位 4：采集P档 P_GEAR_ADC
  Adc_CfgSqrChannel(AdcSQRCH4MUX, P_GEAR_ADC_CH);
  // 扫描槽位 5：采集启停灯 START_STOP_ADC
  Adc_CfgSqrChannel(AdcSQRCH5MUX, START_STOP_ADC_CH);

  // 7. 配置硬件触发源
  // 使用软件触发
  // 8. 全局使能 ADC
  Adc_Enable();
}

// 唤醒后：恢复模拟外设
void DRV_ADC_Wakeup(void) {
    // 1. 先开 BGR，并等待稳定 (必须先开 BGR 才能开 ADC)
    M0P_BGR->CR |= 0x1u;
    delay100us_safe(1); // 等待 BGR 稳定

    // 2. 开启 ADC 电源
    M0P_ADC->CR0 |= 0x1u;
    delay100us_safe(1); // 等待 ADC 稳定

    // 3. 重新使能 ADC 并开启转换
    Adc_Enable();
}

void BSP_ADC_Start(void) {
  // 如果不用定时器触发，可以用这个纯软件触发
  Adc_SQR_Start();
}

void BSP_ADC_Stop(void) { 
	Adc_SQR_Stop(); 
}
