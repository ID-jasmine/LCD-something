#include "main.h"

#include "gpio.h"
#include "wdt.h"
// 睡眠
#include "lpm.h"

#include "bsp_gpio.h"
#include "bsp_sys.h"

#include "drv_adc.h"
#include "drv_can.h"
#include "drv_eeprom.h"
#include "drv_et6934.h"
#include "drv_iic.h"
#include "drv_rtc.h"
#include "drv_time.h"
#include "drv_touch.h"

#include "app_settings.h"
#include "app_ui.h"
#include "app_vehicle.h"

#define model_switch 0

static volatile uint32_t sys_1ms_cnt = 0; // 1ms
static volatile uint8_t rtc_time_1s_flag = 0;
static volatile uint16_t IGN_CNT = 0;
static volatile uint8_t IGN_ON_OFF = 0;		   // 电门
static volatile uint8_t last_ign_state = 0xFF; // 用于记录电门上一次的状态，以捕捉动作瞬间
static volatile uint16_t DeepSleep_cnt = 0, last_wdt_cnt = 0;

int32_t main(void)
{
	SysTick_Init();

	BSP_GPIO_init();
	DRV_IIC_InitAll();
	BSP_GPIO_Unused_Init();
	(void)DRV_EEPROM_Init();
	DRV_ADC_Init();
	(void)DRV_RTC_Init(12, 0); // 明确忽略返回值
	DRV_Touch_Init();
	DRV_CAN_Init();
	WDT_Init();

	while (1)
	{
		// while begin
		//  RTC 时钟读取
		if (rtc_time_1s_flag == 1)
		{
			rtc_time_1s_flag = 0;
			// 只有在非时间设置模式下才从硬件 RTC 同步时间，防止修改时被覆盖
			if (current_display_mode != MODE_TIME_SET_HOUR &&
				current_display_mode != MODE_TIME_SET_MIN)
			{
				stc_rtc_time_t readtime;
				if (Ok == DRV_RTC_ReadDateTime(&readtime))
				{
					second = BCD2DEC(readtime.u8Second);
					minute = BCD2DEC(readtime.u8Minute);
					hour = BCD2DEC(readtime.u8Hour);
				}
			}
		}

		// 边沿触发逻辑
		if (IGN_ON_OFF != last_ign_state)
		{
			if (IGN_ON_OFF == 1)
			{
				// 电门打开：开机、自检初始化
				Gpio_SetIO(GpioPortC, GpioPin4);
				ZiJian_Start = 0;
				// delay_Xms_block(1000);  // 等电压稳定
				DRC_ET6934_Init();
			}
			else
			{
				// 电门关掉：断电、清除状态
				if (last_ign_state == 1)
				{
					DRC_ET6934_ClearAll();
					DRC_ET6934_SetCanvas(true);
					DRC_ET6934_Refresh();
					// 可选：给个极短的延时确保 I2C 数据彻底发送完毕再断电
					delay_Xms_block(10);
				}
				ZiJian_Start = 0;
				is_running = 0;
				is_first_sensor_read = true;
				Gpio_ClrIO(GpioPortC, GpioPin4);
			}
			last_ign_state = IGN_ON_OFF;
		}

		if (IGN_ON_OFF)
		{
#if model_switch == 1
			DRC_ET6934_ClearAll();
			DRC_ET6934_SetAll();
			DRC_ET6934_Refresh();
#else
			__NOP(); // 插入一个单周期的空指令，这里绝对能打上断点,防止优化
			if (ZiJian_Start == 0)
			{
				// 10ms
				static volatile uint32_t last_check_time = 0;
				if (sys_1ms_cnt - last_check_time >= 10)
				{
					last_check_time = sys_1ms_cnt;

					UI_SelfCheck();
				}
			}
			else
			{
				// 自检结束后，进入正常显示逻辑
				static volatile uint32_t last_10ms_time = 0;
				if (sys_1ms_cnt - last_10ms_time >= 10)
				{
					last_10ms_time = sys_1ms_cnt;
					DRV_Touch_Task(); // 按键检测
				}
				static volatile uint32_t last_100ms_time = 0;
				if (sys_1ms_cnt - last_100ms_time >= 100)
				{
					last_100ms_time = sys_1ms_cnt;

					DRV_CAN_Monitor_Task();				 // CAN 超时监控
					SpeedDriver = SpeedDriver_Process(); // 车速调用
					rpmDriver = RpmDriver_Process();	 // 转速调用
					vehicle_sensor_process();			 // 车辆传感器更新ADC
					UI_Button_Task();					 // 处理按键逻辑

					UI_UpdateNormalDisplay(); // 渲染正常内容
				}
			}
#endif
		}
		else
		{
			if (DeepSleep_cnt >= SLEEP_TIME)
			{
				DRV_ADC_DeInit();
				LPM_GPIO_Sleep_Config();
				Wdt_Feed();
				// 在即将休眠的最后一刻，确认一下电门是否为低电平！
				// 避免在清理中断挂起期间用户刚好开电门，导致带着高电平睡死且无法被上升沿唤醒。
				if (Gpio_GetInputIO(GpioPortB, GpioPin0) == 1)
				{
					Lpm_GotoDeepSleep(FALSE);
				}
				// ==========================================
				// 此时系统沉睡，直到 RTC 周期中断 或 外部电门引脚中断 唤醒
				// ==========================================
				// 醒来第一件事：立刻恢复系统高速时钟
				// (因为休眠会自动切回低速主频)
				Sysctrl_SetRCHTrim(SysctrlRchFreq16MHz);
				Sysctrl_ClkSourceEnable(SysctrlClkRCH, TRUE);
				Sysctrl_SysClkSwitch(SysctrlClkRCH);

				Wdt_Feed();

				// 6. 恢复所有外设和 GPIO 状态
				LPM_GPIO_Wakeup_Config(); // 恢复引脚数字功能
				DRV_ADC_Wakeup();		  // 唤醒并稳定 BGR 和 ADC
			}
		}

		// 1s喂狗
		static volatile uint32_t last_wdt_cnt = 0;
		if (sys_1ms_cnt - last_wdt_cnt >= 1000)
		{
			last_wdt_cnt = sys_1ms_cnt;
			Wdt_Feed();
		}

		// while end
	}
}

// 这个接口暴露给所有上层业务用
uint32_t Get_SystemMs(void)
{
	//	uint32_t sys_1ms_temp = 0;
	//	__disable_irq();
	//	sys_1ms_temp = sys_1ms_cnt;
	//	__enable_irq();
	//	这个操作本来就是原子的
	return sys_1ms_cnt;
}

// 内部阻塞延时函数
void delay_Xms_block(volatile uint32_t time)
{
	uint32_t start = DRV_Time_Millis();
	while (DRV_Time_Millis() - start < time)
		; // 死等time时间
}

void SysTick_IRQHandler(void)
{
	// 1ms,一次
	sys_1ms_cnt++;

	// 电门开关检测
	if (Gpio_GetInputIO(GpioPortB, GpioPin0) == 0)
	{
		if (IGN_CNT < 500)
			IGN_CNT++;
		else
		{
			IGN_ON_OFF = 1;
			DeepSleep_cnt = 0;
		}
	}
	else
	{
		if (IGN_CNT > 0)
			IGN_CNT--;
		else
		{
			IGN_ON_OFF = 0;
			if (DeepSleep_cnt < SLEEP_TIME)
				DeepSleep_cnt++;
		}
	}
}

// RTC 中断服务函数
void Rtc_IRQHandler(void)
{
	if (Rtc_GetPridItStatus() == TRUE)
	{
		Rtc_ClearPrdfItStatus();
		rtc_time_1s_flag = 1;
	}
	// 防御性清除闹钟标志，防止死锁
	if (Rtc_GetAlmfItStatus() == TRUE)
	{
		Rtc_ClearAlmfItStatus();
	}
}
