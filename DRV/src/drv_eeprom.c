#include "drv_eeprom.h"
#include "drv_iic.h"

static IIC_Handle_t *s_eeprom_iic = 0;

// begin
static int EEPROM_HW_Init(EepromDevice *dev);
static int EEPROM_HW_ReadBuffer(EepromDevice *dev, uint8_t wordAddress,
                                uint8_t *buffer, uint16_t length);
static int EEPROM_HW_WriteBuffer(EepromDevice *dev, uint8_t wordAddress,
                                 const uint8_t *buffer, uint16_t length);
static IIC_Handle_t *EEPROM_GetIIC(void);

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

// 外部声明你的毫秒获取函数
extern uint32_t Get_SystemMs(void); 

// 内部阻塞延时函数
static void EEPROM_Delay_5ms(void) {
    uint32_t start = Get_SystemMs();
    while (Get_SystemMs() - start < 6); // 死等至少5毫秒以上
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
    DRV_IIC_InitBus(DRV_IIC_BUS_EEPROM);
    s_eeprom_iic = DRV_IIC_GetHandle(DRV_IIC_BUS_EEPROM);
}

void EEPROM_WriteByte(uint8_t wordAddress, uint8_t data) {
    IIC_Handle_t *iic = EEPROM_GetIIC();
    if (iic == 0) {
        return;
    }

    IIC_Start(iic);
    IIC_Send(iic, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(iic);
    IIC_Send(iic, wordAddress);
    IIC_Wait_Ack(iic);
    IIC_Send(iic, data);
    IIC_Wait_Ack(iic);
    IIC_Stop(iic);
    
    EEPROM_Delay_5ms(); // 必须阻塞等待
}

uint8_t EEPROM_ReadByte(uint8_t wordAddress) {
    uint8_t data = 0;
    IIC_Handle_t *iic = EEPROM_GetIIC();
    if (iic == 0) {
        return 0;
    }

    IIC_Start(iic);
    IIC_Send(iic, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(iic);
    IIC_Send(iic, wordAddress);
    IIC_Wait_Ack(iic);

    IIC_Start(iic);
    IIC_Send(iic, EEPROM_ADDR_READ);
    IIC_Wait_Ack(iic);

    data = IIC_ReadByte(iic, 1);
    IIC_Stop(iic);
    return data;
}

// ================= 高级连续读写 =================

void EEPROM_ReadBuffer(uint8_t wordAddress, uint8_t* buffer, uint16_t length) {
    if (length == 0) return;
    IIC_Handle_t *iic = EEPROM_GetIIC();
    if (iic == 0) {
        return;
    }

    IIC_Start(iic);
    IIC_Send(iic, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(iic);
    IIC_Send(iic, wordAddress);
    IIC_Wait_Ack(iic);

    IIC_Start(iic);
    IIC_Send(iic, EEPROM_ADDR_READ);
    IIC_Wait_Ack(iic);

    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = IIC_ReadByte(iic, (i == length - 1) ? 1 : 0);
    }
    IIC_Stop(iic);
}

// 内部单页写入
static void EEPROM_WritePage(uint8_t wordAddress, uint8_t* buffer, uint8_t length) {
    if (length == 0 || length > 16) return;
    IIC_Handle_t *iic = EEPROM_GetIIC();
    if (iic == 0) {
        return;
    }

    IIC_Start(iic);
    IIC_Send(iic, EEPROM_ADDR_WRITE);
    IIC_Wait_Ack(iic);
    IIC_Send(iic, wordAddress);
    IIC_Wait_Ack(iic);
    for (uint8_t i = 0; i < length; i++) {
        IIC_Send(iic, buffer[i]);
        IIC_Wait_Ack(iic);
    }
    IIC_Stop(iic);
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

static IIC_Handle_t *EEPROM_GetIIC(void) {
    if (s_eeprom_iic == 0) {
        EEPROM_Init();
    }
    return s_eeprom_iic;
}
