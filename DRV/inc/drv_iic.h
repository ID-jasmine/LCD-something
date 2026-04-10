#ifndef __DRV_IIC_H
#define __DRV_IIC_H

#include "bsp_iic.h"

typedef enum {
    DRV_IIC_BUS_EEPROM = 0,
    DRV_IIC_BUS_LED1,
    DRV_IIC_BUS_LED2,
    DRV_IIC_BUS_LED3,
    DRV_IIC_BUS_COUNT
} DRV_IIC_BusId_t;

void DRV_IIC_InitBus(DRV_IIC_BusId_t bus_id);
void DRV_IIC_InitAll(void);
IIC_Handle_t *DRV_IIC_GetHandle(DRV_IIC_BusId_t bus_id);

#endif
