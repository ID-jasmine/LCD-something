#include "bsp_sys.h"
#include "flash.h"
#include "wdt.h"

void SysTick_Init(void) {
  stc_sysctrl_clk_cfg_t stcCfg;
  Sysctrl_SetPeripheralGate(SysctrlPeripheralFlash, TRUE);
  Flash_WaitCycle(FlashWaitCycle0);
  Sysctrl_SetRCHTrim(SysctrlRchFreq16MHz);

  stcCfg.enClkSrc = SysctrlClkRCH;
  stcCfg.enHClkDiv = SysctrlHclkDiv1;
  stcCfg.enPClkDiv = SysctrlPclkDiv1;
  Sysctrl_ClkInit(&stcCfg);
  SysTick_Config(16000); // 系统1ms中断
}

void WDT_Init(void) {
  Sysctrl_SetPeripheralGate(SysctrlPeripheralWdt, TRUE);
	// 开机默认初始化为54.4s，保证正常工作时的安全性
  Wdt_Init(WdtResetEn, WdtT52s4);
  Wdt_Start();
  Wdt_Feed(); // 喂狗
}

void delay100us_safe(uint32_t u32Cnt) {
  uint32_t ticks_per_100us = 1600;
  uint32_t t0, t1;

  while (u32Cnt-- > 0) {
    t0 = SysTick->VAL;
    while (1) {
      t1 = SysTick->VAL;
      if (t0 < t1) {
        if ((t0 + (SysTick->LOAD - t1)) >= ticks_per_100us)
          break;
      } else {
        if ((t0 - t1) >= ticks_per_100us)
          break;
      }
    }
  }
}
