#ifndef __DRV_EEPROM_H
#define __DRV_EEPROM_H

#include "bsp_iic.h"
#include <stdint.h>

// 硬件设备地址
#define EEPROM_ADDR_WRITE 0xA0
#define EEPROM_ADDR_READ  0xA1

void EEPROM_Init(void);
void EEPROM_WriteByte(uint8_t wordAddress, uint8_t data);
uint8_t EEPROM_ReadByte(uint8_t wordAddress);

// 高级连续读写接口 (带智能分页)
void EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length);
void EEPROM_WriteBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length);

#endif
