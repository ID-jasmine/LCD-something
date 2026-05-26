#include "drv_eeprom.h"
#include "drv_iic.h"
#include "drv_time.h"

#define EEPROM_ADDR_WRITE 0xA0
#define EEPROM_ADDR_READ  0xA1

typedef struct {
	DRV_IIC_Bus *bus;
	uint8_t addr_write;
	uint8_t addr_read;
} EepromContext;

static int EEPROM_Device_Init(EepromDevice *dev);
static int EEPROM_Device_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
									uint8_t *buffer, uint16_t length);
static int EEPROM_Device_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
									 const uint8_t *buffer, uint16_t length);
static int EEPROM_HW_Init(EepromDevice *dev);
static int EEPROM_HW_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
								uint8_t *buffer, uint16_t length);
static int EEPROM_HW_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
								 const uint8_t *buffer, uint16_t length);
static int EEPROM_HW_WritePage(EepromDevice *dev, uint8_t wordAddress,
							   const uint8_t *buffer, uint8_t length);
static DRV_IIC_Bus *EEPROM_GetBus(EepromDevice *dev);
static EepromDevice *EEPROM_GetDefaultDevice(void);
static void EEPROM_Delay_5ms(void);

static const EepromOps s_eeprom_ops = {
	.init = EEPROM_HW_Init,
	.read_buffer = EEPROM_HW_ReadBuffer,
	.write_buffer = EEPROM_HW_WriteBuffer,
};

static EepromContext s_eeprom_context = {
	.bus = 0,
	.addr_write = EEPROM_ADDR_WRITE,
	.addr_read = EEPROM_ADDR_READ,
};

static EepromDevice s_eeprom_dev = {
	.ops = &s_eeprom_ops,
	.context = &s_eeprom_context,
	.page_size = 16,
};

int DRV_EEPROM_Init(void) {
	return EEPROM_Device_Init(EEPROM_GetDefaultDevice());
}

int DRV_EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t *buffer, uint16_t length) {
	return EEPROM_Device_ReadBuffer(EEPROM_GetDefaultDevice(), wordAddress, buffer,
									length);
}

int DRV_EEPROM_WriteBuffer(uint8_t wordAddress, const uint8_t *buffer,
						   uint16_t length) {
	return EEPROM_Device_WriteBuffer(EEPROM_GetDefaultDevice(), wordAddress, buffer,
									 length);
}

static int EEPROM_Device_Init(EepromDevice *dev) {
	if (dev == 0 || dev->ops == 0 || dev->ops->init == 0) {
		return -1;
	}
	return dev->ops->init(dev);
}

static int EEPROM_Device_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
									uint8_t *buffer, uint16_t length) {
	if (dev == 0 || dev->ops == 0 || dev->ops->read_buffer == 0) {
		return -1;
	}
	return dev->ops->read_buffer(dev, wordAddress, buffer, length);
}

static int EEPROM_Device_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
									 const uint8_t *buffer, uint16_t length) {
	if (dev == 0 || dev->ops == 0 || dev->ops->write_buffer == 0) {
		return -1;
	}
	return dev->ops->write_buffer(dev, wordAddress, buffer, length);
}

static int EEPROM_HW_Init(EepromDevice *dev) {
	EepromContext *ctx;

	if (dev == 0 || dev->context == 0) {
		return -1;
	}

	ctx = (EepromContext *)dev->context;
	ctx->bus = DRV_IIC_GetBus(DRV_IIC_BUS_EEPROM);
	if (ctx->bus == 0) {
		return -1;
	}

	(void)DRV_IIC_Bus_Init(ctx->bus);
	return 0;
}

static int EEPROM_HW_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
								uint8_t *buffer, uint16_t length) {
	DRV_IIC_Bus *bus;
	EepromContext *ctx;

	if (buffer == 0) {
		return -1;
	}
	if (length == 0) {
		return 0;
	}

	bus = EEPROM_GetBus(dev);
	if (bus == 0 || bus->ops == 0 || dev == 0 || dev->context == 0) {
		return -1;
	}

	ctx = (EepromContext *)dev->context;
	bus->ops->start(bus);
	bus->ops->send(bus, ctx->addr_write);
	bus->ops->wait_ack(bus);
	bus->ops->send(bus, wordAddress);
	bus->ops->wait_ack(bus);

	bus->ops->start(bus);
	bus->ops->send(bus, ctx->addr_read);
	bus->ops->wait_ack(bus);

	for (uint16_t i = 0; i < length; i++) {
		buffer[i] = bus->ops->read_byte(bus, (i == length - 1) ? 1 : 0);
	}
	bus->ops->stop(bus);
	return 0;
}

static int EEPROM_HW_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
								 const uint8_t *buffer, uint16_t length) {
	uint8_t page_remain;

	if (dev == 0 || buffer == 0 || dev->page_size == 0) {
		return -1;
	}
	if (length == 0) {
		return 0;
	}

	page_remain = dev->page_size - (wordAddress % dev->page_size);
	if (length <= page_remain) {
		page_remain = length;
	}

	while (1) {
		if (EEPROM_HW_WritePage(dev, wordAddress, buffer, page_remain) != 0) {
			return -1;
		}
		if (length == page_remain) {
			break;
		}

		wordAddress += page_remain;
		buffer += page_remain;
		length -= page_remain;
		page_remain = (length > dev->page_size) ? dev->page_size : length;
	}
	return 0;
}

static int EEPROM_HW_WritePage(EepromDevice *dev, uint8_t wordAddress,
							   const uint8_t *buffer, uint8_t length) {
	DRV_IIC_Bus *bus;
	EepromContext *ctx;

	if (dev == 0 || dev->context == 0 || buffer == 0 || length == 0 ||
		length > dev->page_size) {
		return -1;
	}

	bus = EEPROM_GetBus(dev);
	if (bus == 0 || bus->ops == 0) {
		return -1;
	}

	ctx = (EepromContext *)dev->context;
	bus->ops->start(bus);
	bus->ops->send(bus, ctx->addr_write);
	bus->ops->wait_ack(bus);
	bus->ops->send(bus, wordAddress);
	bus->ops->wait_ack(bus);
	for (uint8_t i = 0; i < length; i++) {
		bus->ops->send(bus, buffer[i]);
		bus->ops->wait_ack(bus);
	}
	bus->ops->stop(bus);

	EEPROM_Delay_5ms();
	return 0;
}

static DRV_IIC_Bus *EEPROM_GetBus(EepromDevice *dev) {
	EepromContext *ctx;

	if (dev == 0 || dev->context == 0) {
		return 0;
	}

	ctx = (EepromContext *)dev->context;
	if (ctx->bus == 0) {
		(void)EEPROM_HW_Init(dev);
	}
	return ctx->bus;
}

static EepromDevice *EEPROM_GetDefaultDevice(void) {
	return &s_eeprom_dev;
}

static void EEPROM_Delay_5ms(void) {
	uint32_t start = DRV_Time_Millis();
	while (DRV_Time_Millis() - start < 6) {
	}
}
