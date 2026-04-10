#include "drv_eeprom.h"

IIC_Handle_t EEPROM_IIC_Handle;

static int EEPROM_HW_Init(EepromDevice *dev);
static int EEPROM_HW_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
                                uint8_t *buffer, uint16_t length);
static int EEPROM_HW_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
                                 const uint8_t *buffer, uint16_t length);

static const EepromOps s_eeprom_ops = {
    .init = EEPROM_HW_Init,
    .read_buffer = EEPROM_HW_ReadBuffer,
    .write_buffer = EEPROM_HW_WriteBuffer,
};

EepromDevice g_eeprom_dev = {
    .ops = &s_eeprom_ops,
    .context = 0,
    .page_size = 16,
};

// 外部声明你的毫秒获取函数
extern uint32_t Get_SystemMs(void); 

// 内部阻塞延时函数
static void EEPROM_Delay_5ms(void) {
    uint32_t start = Get_SystemMs();
    while (Get_SystemMs() - start < 6); // 死等至少5毫秒以上
}

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
    EEPROM_IIC_Handle.scl_port = GpioPortB; 
    EEPROM_IIC_Handle.scl_pin  = GpioPin10;
    EEPROM_IIC_Handle.sda_port = GpioPortB;
    EEPROM_IIC_Handle.sda_pin  = GpioPin11;
    IIC_GPIO_Init(&EEPROM_IIC_Handle);
}

void EEPROM_WriteByte(uint8_t wordAddress, uint8_t data) {
    IIC_Start(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, EEPROM_ADDR_WRITE); 
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, wordAddress);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, data);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    IIC_Stop(&EEPROM_IIC_Handle);
    
    EEPROM_Delay_5ms(); // 必须阻塞等待
}

uint8_t EEPROM_ReadByte(uint8_t wordAddress) {
    uint8_t data = 0;
    IIC_Start(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, wordAddress);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    
    IIC_Start(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, EEPROM_ADDR_READ);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    
    data = IIC_ReadByte(&EEPROM_IIC_Handle, 1); 
    IIC_Stop(&EEPROM_IIC_Handle);
    return data;
}

// ================= 高级连续读写 =================

void EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length) {
    if (length == 0) return;
    IIC_Start(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, wordAddress);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    
    IIC_Start(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, EEPROM_ADDR_READ);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    
    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = IIC_ReadByte(&EEPROM_IIC_Handle, (i == length - 1) ? 1 : 0); 
    }
    IIC_Stop(&EEPROM_IIC_Handle);
}

// 内部单页写入
static void EEPROM_WritePage(uint8_t wordAddress, uint8_t* buffer, uint8_t length) {
    if (length == 0 || length > 16) return; 
    IIC_Start(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    IIC_Send(&EEPROM_IIC_Handle, wordAddress);
    IIC_Wait_Ack(&EEPROM_IIC_Handle);
    for (uint8_t i = 0; i < length; i++) {
        IIC_Send(&EEPROM_IIC_Handle, buffer[i]);
        IIC_Wait_Ack(&EEPROM_IIC_Handle);
    }
    IIC_Stop(&EEPROM_IIC_Handle);
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
