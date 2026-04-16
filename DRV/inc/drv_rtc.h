#ifndef __DRV_RTC_H
#define __DRV_RTC_H

#include "rtc.h"

typedef struct RTC_Device RTC_Device;

typedef struct {
    int (*init)(RTC_Device *dev, uint8_t hour, uint8_t minute);
    en_result_t (*read_datetime)(RTC_Device *dev, stc_rtc_time_t *time);
    en_result_t (*set_time)(RTC_Device *dev, const stc_rtc_time_t *time);
} RTC_Ops;

struct RTC_Device {
    const RTC_Ops *ops;
    void *context;
};

extern RTC_Device g_rtc_dev;

// api
int DRV_RTC_Init(uint8_t hour, uint8_t minute);
en_result_t DRV_RTC_ReadDateTime(stc_rtc_time_t *time);
en_result_t DRV_RTC_SetTime(const stc_rtc_time_t *time);

#endif
