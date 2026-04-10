#include "drv_iic.h"

static IIC_Handle_t s_iic_buses[DRV_IIC_BUS_COUNT] = {
    {
        .scl_port = GpioPortB,
        .scl_pin = GpioPin10,
        .sda_port = GpioPortB,
        .sda_pin = GpioPin11,
    },
    {
        .scl_port = GpioPortA,
        .scl_pin = GpioPin7,
        .sda_port = GpioPortA,
        .sda_pin = GpioPin6,
    },
    {
        .scl_port = GpioPortB,
        .scl_pin = GpioPin7,
        .sda_port = GpioPortB,
        .sda_pin = GpioPin6,
    },
    {
        .scl_port = GpioPortA,
        .scl_pin = GpioPin5,
        .sda_port = GpioPortA,
        .sda_pin = GpioPin4,
    },
};

void DRV_IIC_InitBus(DRV_IIC_BusId_t bus_id) {
    if (bus_id >= DRV_IIC_BUS_COUNT) {
        return;
    }
    IIC_GPIO_Init(&s_iic_buses[bus_id]);
}

void DRV_IIC_InitAll(void) {
    for (uint8_t i = 0; i < (uint8_t)DRV_IIC_BUS_COUNT; i++) {
        DRV_IIC_InitBus((DRV_IIC_BusId_t)i);
    }
}

IIC_Handle_t *DRV_IIC_GetHandle(DRV_IIC_BusId_t bus_id) {
    if (bus_id >= DRV_IIC_BUS_COUNT) {
        return 0;
    }
    return &s_iic_buses[bus_id];
}
