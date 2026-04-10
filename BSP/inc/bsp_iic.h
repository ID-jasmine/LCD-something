#ifndef __BSP_IIC_H
#define __BSP_IIC_H

#include "ddl.h"
#include "gpio.h"

// 1. 定义 IIC 句柄结构体，包含 SCL 和 SDA 的端口与引脚
typedef struct {
    en_gpio_port_t scl_port;
    en_gpio_pin_t  scl_pin;
    en_gpio_port_t sda_port;
    en_gpio_pin_t  sda_pin;
} IIC_Handle_t;

// 2. 函数声明：所有函数均需传入句柄指针
void IIC_GPIO_Init(IIC_Handle_t* iic);
void IIC_Start(IIC_Handle_t* iic);
void IIC_Stop(IIC_Handle_t* iic);
void IIC_Send(IIC_Handle_t* iic, uint8_t data);
void IIC_Wait_Ack(IIC_Handle_t* iic);
uint8_t IIC_ReadByte(IIC_Handle_t* iic, uint8_t ack);

#endif
