#ifndef __DRV_EEPROM_H
#define __DRV_EEPROM_H

#include <stdint.h>

// 硬件设备地址
#define EEPROM_ADDR_WRITE 0xA0
#define EEPROM_ADDR_READ  0xA1

typedef struct EepromDevice EepromDevice;

// eeprom操作集(虚拟函数表)
typedef struct {
	int (*init)(EepromDevice *dev);
	int (*read_buffer)(EepromDevice *dev, uint8_t wordAddress, uint8_t *buffer,
					   uint16_t length);
	int (*write_buffer)(EepromDevice *dev, uint8_t wordAddress,
						const uint8_t *buffer, uint16_t length);
} EepromOps;

// 通用设备对象
struct EepromDevice {
	const EepromOps *ops;   
	void *context;
	uint8_t page_size;
};

extern EepromDevice g_eeprom_dev;

int EEPROM_Device_Init(EepromDevice *dev);
int EEPROM_Device_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
							 uint8_t *buffer, uint16_t length);
int EEPROM_Device_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
							  const uint8_t *buffer, uint16_t length);

void EEPROM_Init(void);
void EEPROM_WriteByte(uint8_t wordAddress, uint8_t data);
uint8_t EEPROM_ReadByte(uint8_t wordAddress);

// 高级连续读写接口 (带智能分页)
void EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length);
void EEPROM_WriteBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length);                              

#endif
