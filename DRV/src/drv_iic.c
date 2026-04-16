#include "drv_iic.h"
#include "gpio.h"

static int DRV_IIC_HW_Init(DRV_IIC_Bus *bus);
static void DRV_IIC_HW_Start(DRV_IIC_Bus *bus);
static void DRV_IIC_HW_Stop(DRV_IIC_Bus *bus);
static void DRV_IIC_HW_Send(DRV_IIC_Bus *bus, uint8_t data);
static void DRV_IIC_HW_WaitAck(DRV_IIC_Bus *bus);
static uint8_t DRV_IIC_HW_ReadByte(DRV_IIC_Bus *bus, uint8_t ack);
static void DRV_IIC_ApplyDefaultConfig(void);

static const DRV_IIC_BusOps s_iic_bus_ops = {
    .init = DRV_IIC_HW_Init,
    .start = DRV_IIC_HW_Start,
    .stop = DRV_IIC_HW_Stop,
    .send = DRV_IIC_HW_Send,
    .wait_ack = DRV_IIC_HW_WaitAck,
    .read_byte = DRV_IIC_HW_ReadByte,
};
//加硬件操作iic时，最科学的做法是定义一个新的操作集(包括里面的函数)，给新的对象绑定新的操作集

static DRV_IIC_Bus s_iic_buses[DRV_IIC_BUS_COUNT];
static uint8_t s_iic_bus_cfg_done = 0;

int DRV_IIC_Bus_Init(DRV_IIC_Bus *bus) {
    if (bus == 0 || bus->ops == 0 || bus->ops->init == 0) {
        return -1;
    }
    return bus->ops->init(bus);
}

DRV_IIC_Bus *DRV_IIC_GetBus(DRV_IIC_BusId_t bus_id) {
    DRV_IIC_ApplyDefaultConfig();
    if (bus_id >= DRV_IIC_BUS_COUNT) {
        return 0;
    }
    return &s_iic_buses[bus_id];
}

void DRV_IIC_InitBus(DRV_IIC_BusId_t bus_id) {
    DRV_IIC_Bus *bus = DRV_IIC_GetBus(bus_id);
    if (bus == 0) {
        return;
    }
    (void)DRV_IIC_Bus_Init(bus);
}

void DRV_IIC_InitAll(void) {
    for (uint8_t i = 0; i < (uint8_t)DRV_IIC_BUS_COUNT; i++) {
        DRV_IIC_InitBus((DRV_IIC_BusId_t)i);
    }
}

// 向后兼容接口，直接返回对应总线的句柄指针
// IIC_Handle_t *DRV_IIC_GetHandle(DRV_IIC_BusId_t bus_id) {
//     DRV_IIC_Bus *bus = DRV_IIC_GetBus(bus_id);
//     if (bus == 0) {
//         return 0;
//     }
//     return &bus->handle;
// }

static int DRV_IIC_HW_Init(DRV_IIC_Bus *bus) {
    IIC_GPIO_Init(&bus->handle);
    return 0;
}

static void DRV_IIC_HW_Start(DRV_IIC_Bus *bus) { IIC_Start(&bus->handle); }

static void DRV_IIC_HW_Stop(DRV_IIC_Bus *bus) { IIC_Stop(&bus->handle); }

static void DRV_IIC_HW_Send(DRV_IIC_Bus *bus, uint8_t data) {
    IIC_Send(&bus->handle, data);
}

static void DRV_IIC_HW_WaitAck(DRV_IIC_Bus *bus) { IIC_Wait_Ack(&bus->handle); }

static uint8_t DRV_IIC_HW_ReadByte(DRV_IIC_Bus *bus, uint8_t ack) {
    return IIC_ReadByte(&bus->handle, ack);
}

static void DRV_IIC_ApplyDefaultConfig(void) {
    if (s_iic_bus_cfg_done) {
        return;
    }

    s_iic_buses[DRV_IIC_BUS_EEPROM].handle.scl_port = GpioPortB;
    s_iic_buses[DRV_IIC_BUS_EEPROM].handle.scl_pin = GpioPin10;
    s_iic_buses[DRV_IIC_BUS_EEPROM].handle.sda_port = GpioPortB;
    s_iic_buses[DRV_IIC_BUS_EEPROM].handle.sda_pin = GpioPin11;

    s_iic_buses[DRV_IIC_BUS_LED1].handle.scl_port = GpioPortA;
    s_iic_buses[DRV_IIC_BUS_LED1].handle.scl_pin = GpioPin7;
    s_iic_buses[DRV_IIC_BUS_LED1].handle.sda_port = GpioPortA;
    s_iic_buses[DRV_IIC_BUS_LED1].handle.sda_pin = GpioPin6;

    s_iic_buses[DRV_IIC_BUS_LED2].handle.scl_port = GpioPortB;
    s_iic_buses[DRV_IIC_BUS_LED2].handle.scl_pin = GpioPin7;
    s_iic_buses[DRV_IIC_BUS_LED2].handle.sda_port = GpioPortB;
    s_iic_buses[DRV_IIC_BUS_LED2].handle.sda_pin = GpioPin6;

    s_iic_buses[DRV_IIC_BUS_LED3].handle.scl_port = GpioPortA;
    s_iic_buses[DRV_IIC_BUS_LED3].handle.scl_pin = GpioPin5;
    s_iic_buses[DRV_IIC_BUS_LED3].handle.sda_port = GpioPortA;
    s_iic_buses[DRV_IIC_BUS_LED3].handle.sda_pin = GpioPin4;

    s_iic_buses[DRV_IIC_BUS_EEPROM].ops = &s_iic_bus_ops;
    s_iic_buses[DRV_IIC_BUS_LED1].ops = &s_iic_bus_ops;
    s_iic_buses[DRV_IIC_BUS_LED2].ops = &s_iic_bus_ops;
    s_iic_buses[DRV_IIC_BUS_LED3].ops = &s_iic_bus_ops;

    s_iic_buses[DRV_IIC_BUS_EEPROM].context = 0;
    s_iic_buses[DRV_IIC_BUS_LED1].context = 0;
    s_iic_buses[DRV_IIC_BUS_LED2].context = 0;
    s_iic_buses[DRV_IIC_BUS_LED3].context = 0;

    s_iic_bus_cfg_done = 1;
}
