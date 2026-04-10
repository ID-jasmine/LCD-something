#ifndef __DRV_IIC_H
#define __DRV_IIC_H

#include "bsp_iic.h"

typedef struct DRV_IIC_Bus DRV_IIC_Bus;

typedef struct {
    int (*init)(DRV_IIC_Bus *bus);
    void (*start)(DRV_IIC_Bus *bus);
    void (*stop)(DRV_IIC_Bus *bus);
    void (*send)(DRV_IIC_Bus *bus, uint8_t data);
    void (*wait_ack)(DRV_IIC_Bus *bus);
    uint8_t (*read_byte)(DRV_IIC_Bus *bus, uint8_t ack);
} DRV_IIC_BusOps;

struct DRV_IIC_Bus {
    IIC_Handle_t handle;
    const DRV_IIC_BusOps *ops;
    void *context;
};

typedef enum {
    DRV_IIC_BUS_EEPROM = 0,
    DRV_IIC_BUS_LED1,
    DRV_IIC_BUS_LED2,
    DRV_IIC_BUS_LED3,
    DRV_IIC_BUS_COUNT
} DRV_IIC_BusId_t;

int DRV_IIC_Bus_Init(DRV_IIC_Bus *bus);
DRV_IIC_Bus *DRV_IIC_GetBus(DRV_IIC_BusId_t bus_id);

void DRV_IIC_InitBus(DRV_IIC_BusId_t bus_id);
void DRV_IIC_InitAll(void);
IIC_Handle_t *DRV_IIC_GetHandle(DRV_IIC_BusId_t bus_id);

#endif
