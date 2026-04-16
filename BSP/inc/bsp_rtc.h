#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "ddl.h"
#include "rtc.h"

void BSP_RTC_Init(uint8_t Hour, uint8_t Minute);
en_result_t BSP_RTC_ReadDateTime(stc_rtc_time_t *time);
en_result_t BSP_RTC_SetTime(const stc_rtc_time_t *time);

#endif
