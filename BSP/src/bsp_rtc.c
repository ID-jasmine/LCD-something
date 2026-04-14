#include "bsp_rtc.h"
#include "drv_time.h"
#include "wdt.h"


void BSP_RTC_Init(uint8_t Hour, uint8_t Minute) {
	Sysctrl_ClkSourceEnable(SysctrlClkXTL, TRUE); 	
	// 开启 RTC 时钟
	Sysctrl_SetPeripheralGate(SysctrlPeripheralRtc, TRUE); 

	uint32_t start_ms = DRV_Time_Millis();
	uint8_t count = 0;
	while ((DRV_Time_Millis() - start_ms) < 500) {
		count++;
		Wdt_Feed();
		if(count >= 4)break;
	}

	stc_rtc_initstruct_t RtcInitStruct;

	RtcInitStruct.rtcAmpm = RtcPm;
	RtcInitStruct.rtcClksrc = RtcClkXtl; 
	RtcInitStruct.rtcPrdsel.rtcPrdsel = RtcPrdx;
	RtcInitStruct.rtcPrdsel.rtcPrdx = 1u; // 1秒周期

	RtcInitStruct.rtcTime.u8Second = 0;
	RtcInitStruct.rtcTime.u8Minute = DEC2BCD(Minute);
	RtcInitStruct.rtcTime.u8Hour = DEC2BCD(Hour);
	RtcInitStruct.rtcTime.u8DayOfWeek = 1; // 星期一
	RtcInitStruct.rtcTime.u8Day       = 1; // 1日
	RtcInitStruct.rtcTime.u8Month     = 1; // 1月
	RtcInitStruct.rtcTime.u8Year      = 0; // 00年

	RtcInitStruct.rtcCompen = RtcCompenEnable;
	RtcInitStruct.rtcCompValue = 0;

	Rtc_Init(&RtcInitStruct); 
	Rtc_Cmd(TRUE); // 启动 RTC 计数

	Rtc_ClearPrdfItStatus(); 	
	Rtc_ClearAlmfItStatus(); 	
	Rtc_AlmIeCmd(TRUE); 			
	EnableNvic(RTC_IRQn, IrqLevel3, TRUE);
}
