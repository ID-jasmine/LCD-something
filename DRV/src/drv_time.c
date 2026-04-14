#include "drv_time.h"

extern uint32_t Get_SystemMs(void);

uint32_t DRV_Time_Millis(void) {
    return Get_SystemMs();
}