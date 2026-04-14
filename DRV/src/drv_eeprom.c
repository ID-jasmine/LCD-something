#include "drv_eeprom.h"
#include "drv_iic.h"
#include "drv_time.h"

static DRV_IIC_Bus *s_eeprom_bus = 0;

// begin
static int EEPROM_HW_Init(EepromDevice *dev);
static int EEPROM_HW_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
                                uint8_t *buffer, uint16_t length);
static int EEPROM_HW_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
                                 const uint8_t *buffer, uint16_t length);
static DRV_IIC_Bus *EEPROM_GetBus(void);

// 实现操作集，化虚为实
static const EepromOps s_eeprom_ops = {
    .init = EEPROM_HW_Init,
    .read_buffer = EEPROM_HW_ReadBuffer,
    .write_buffer = EEPROM_HW_WriteBuffer,
};

// 实例化全局设备对象(this)
EepromDevice g_eeprom_dev = {
    .ops = &s_eeprom_ops,
    .context = 0,
    .page_size = 16,
};
// end

// 内部阻塞延时函数
static void EEPROM_Delay_5ms(void) {
    uint32_t start = DRV_Time_Millis();
    while (DRV_Time_Millis() - start < 6); // 死等至少5毫秒以上
}

// api接口实现
int EEPROM_Device_Init(EepromDevice *dev) {
    if (dev == 0 || dev->ops == 0 || dev->ops->init == 0) {
        return -1;
    }
    return dev->ops->init(dev);
}

int EEPROM_Device_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
                             uint8_t *buffer, uint16_t length) {
    if (dev == 0 || dev->ops == 0 || dev->ops->read_buffer == 0) {
        return -1;
    }
    return dev->ops->read_buffer(dev, wordAddress, buffer, length);
}

int EEPROM_Device_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
                              const uint8_t *buffer, uint16_t length) {
    if (dev == 0 || dev->ops == 0 || dev->ops->write_buffer == 0) {
        return -1;
    }
    return dev->ops->write_buffer(dev, wordAddress, buffer, length);
}

// 函数实现
static int EEPROM_HW_Init(EepromDevice *dev) {
    (void)dev;
    EEPROM_Init();
    return 0;
}

static int EEPROM_HW_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
                                uint8_t *buffer, uint16_t length) {
    (void)dev;
    if (buffer == 0) {
        return -1;
    }
    EEPROM_ReadBuffer(wordAddress, buffer, length);
    return 0;
}

static int EEPROM_HW_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
                                 const uint8_t *buffer, uint16_t length) {
    (void)dev;
    if (buffer == 0) {
        return -1;
    }
    EEPROM_WriteBuffer(wordAddress, (uint8_t *)buffer, length);
    return 0;
}

void EEPROM_Init(void) {
    s_eeprom_bus = DRV_IIC_GetBus(DRV_IIC_BUS_EEPROM);
    if (s_eeprom_bus == 0) {
        return;
    }
    (void)DRV_IIC_Bus_Init(s_eeprom_bus);
}

void EEPROM_WriteByte(uint8_t wordAddress, uint8_t data) {
    DRV_IIC_Bus *bus = EEPROM_GetBus();
    if (bus == 0 || bus->ops == 0) {
        return;
    }

    bus->ops->start(bus);
    bus->ops->send(bus, EEPROM_ADDR_WRITE);
    bus->ops->wait_ack(bus);
    bus->ops->send(bus, wordAddress);
    bus->ops->wait_ack(bus);
    bus->ops->send(bus, data);
    bus->ops->wait_ack(bus);
    bus->ops->stop(bus);
    
    EEPROM_Delay_5ms(); // 必须阻塞等待
}

uint8_t EEPROM_ReadByte(uint8_t wordAddress) {
    uint8_t data = 0;
    DRV_IIC_Bus *bus = EEPROM_GetBus();
    if (bus == 0 || bus->ops == 0) {
        return 0;
    }

    bus->ops->start(bus);
    bus->ops->send(bus, EEPROM_ADDR_WRITE);
    bus->ops->wait_ack(bus);
    bus->ops->send(bus, wordAddress);
    bus->ops->wait_ack(bus);

    bus->ops->start(bus);
    bus->ops->send(bus, EEPROM_ADDR_READ);
    bus->ops->wait_ack(bus);

    data = bus->ops->read_byte(bus, 1);
    bus->ops->stop(bus);
    return data;
}

// ================= 高级连续读写 =================

void EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length) {
    if (length == 0) return;
    DRV_IIC_Bus *bus = EEPROM_GetBus();
    if (bus == 0 || bus->ops == 0) {
        return;
    }

    bus->ops->start(bus);
    bus->ops->send(bus, EEPROM_ADDR_WRITE);
    bus->ops->wait_ack(bus);
    bus->ops->send(bus, wordAddress);
    bus->ops->wait_ack(bus);

    bus->ops->start(bus);
    bus->ops->send(bus, EEPROM_ADDR_READ);
    bus->ops->wait_ack(bus);

    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = bus->ops->read_byte(bus, (i == length - 1) ? 1 : 0);
    }
    bus->ops->stop(bus);
}

// 内部单页写入
static void EEPROM_WritePage(uint8_t wordAddress, uint8_t* buffer, uint8_t length) {
    if (length == 0 || length > 16) return;
    DRV_IIC_Bus *bus = EEPROM_GetBus();
    if (bus == 0 || bus->ops == 0) {
        return;
    }

    bus->ops->start(bus);
    bus->ops->send(bus, EEPROM_ADDR_WRITE);
    bus->ops->wait_ack(bus);
    bus->ops->send(bus, wordAddress);
    bus->ops->wait_ack(bus);
    for (uint8_t i = 0; i < length; i++) {
        bus->ops->send(bus, buffer[i]);
        bus->ops->wait_ack(bus);
    }
    bus->ops->stop(bus);
    EEPROM_Delay_5ms(); // 一页只需等1次
}

// 智能跨页写入 (外部直接调用这个)
void EEPROM_WriteBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length) {
    uint8_t pageRemain = 16 - (wordAddress % 16); 
    if (length <= pageRemain) pageRemain = length; 

    while (1) {
        EEPROM_WritePage(wordAddress, buffer, pageRemain);
        if (length == pageRemain) break; 
        
        wordAddress += pageRemain; 
        buffer += pageRemain;      
        length -= pageRemain;      
        pageRemain = (length > 16) ? 16 : length;
    }
}

static DRV_IIC_Bus *EEPROM_GetBus(void) {
    if (s_eeprom_bus == 0) {
        EEPROM_Init();
    }
    return s_eeprom_bus;
}
