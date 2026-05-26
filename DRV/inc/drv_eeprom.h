#ifndef __DRV_EEPROM_H
#define __DRV_EEPROM_H

#include <stdint.h>

// 声明结构体类型
typedef struct EepromDevice EepromDevice;

// eeprom操作集(虚拟函数表)
typedef struct {
	int (*init)(EepromDevice *dev);
	int (*read_buffer)(EepromDevice *dev, uint8_t wordAddress, uint8_t *buffer,
					   uint16_t length);
	int (*write_buffer)(EepromDevice *dev, uint8_t wordAddress, const uint8_t *buffer,
						uint16_t length);
} EepromOps;

// 通用设备对象
struct EepromDevice {
	const EepromOps *ops;
	void *context;
	uint8_t page_size;
};

// 默认实例门面接口（推荐给上层使用）
int DRV_EEPROM_Init(void);
int DRV_EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t *buffer, uint16_t length);
int DRV_EEPROM_WriteBuffer(uint8_t wordAddress, const uint8_t *buffer, uint16_t length);

#endif
