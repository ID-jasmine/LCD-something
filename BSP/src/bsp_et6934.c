#include "bsp_et6934.h"

// 内部函数 1：单次写寄存器
void ET6934_Write_Reg(ET6934_Handle_t *dev, uint8_t reg_addr, uint8_t data) {
    IIC_Start(dev->iic);
    IIC_Send(dev->iic, dev->dev_addr);
    IIC_Wait_Ack(dev->iic);

    IIC_Send(dev->iic, reg_addr);
    IIC_Wait_Ack(dev->iic);

    IIC_Send(dev->iic, data);
    IIC_Wait_Ack(dev->iic);

    IIC_Stop(dev->iic);
}

// 内部函数 2：连续写显存
static void ET6934_Fill_RAM(ET6934_Handle_t *dev, uint8_t data) {
    IIC_Start(dev->iic);
    IIC_Send(dev->iic, dev->dev_addr);
    IIC_Wait_Ack(dev->iic);

    // 从显存起始地址开始写入
    IIC_Send(dev->iic, ET6934_REG_DATA_START);
    IIC_Wait_Ack(dev->iic);

    // 连续写入 16 个字节，对应 16 个 GRID
    for (uint8_t i = 0; i < 16; i++) {
        IIC_Send(dev->iic, data);
        IIC_Wait_Ack(dev->iic);
    }

    IIC_Stop(dev->iic);
}

void ET6934_Refresh_RAM(ET6934_Handle_t *dev, uint8_t *pBuffer) {
    IIC_Start(dev->iic);
    IIC_Send(dev->iic, dev->dev_addr);
    IIC_Wait_Ack(dev->iic);

    IIC_Send(dev->iic, ET6934_REG_DATA_START);
    IIC_Wait_Ack(dev->iic);

    for (uint8_t i = 0; i < 16; i++) {
        IIC_Send(dev->iic, pBuffer[i]); // <--- 改为发送数组里的不同数据
        IIC_Wait_Ack(dev->iic);
    }
    IIC_Stop(dev->iic);
}

// 1. 屏幕初始化 (遵循手册 Command Order 规范)
void ET6934_Init(ET6934_Handle_t *dev, IIC_Handle_t *iic_handle, uint8_t addr) {
    dev->iic = iic_handle;
    dev->dev_addr = addr;

    IIC_GPIO_Init(dev->iic);

    // 第一步：写入状态控制寄存器，进入工作状态，但保持显示关闭 [cite: 243]
    ET6934_Write_Reg(dev, ET6934_REG_CTRL, ET6934_CTRL_SYS_ON_DISP_OFF);

    // 第二步：写入显示数据，上电先清空显存 [cite: 244]
    ET6934_All_Off(dev);

    // 第三步：配置显示模式寄存器 (亮度和扫描行数) [cite: 244]
    ET6934_Write_Reg(dev, ET6934_REG_MODE_1, ET6934_MODE1_MAX_BRIGHT);
    ET6934_Write_Reg(dev, ET6934_REG_MODE_2, ET6934_MODE2_16_ROWS);

    // 第四步：写入状态控制寄存器，正式打开显示 [cite: 244]
    ET6934_Write_Reg(dev, ET6934_REG_CTRL, ET6934_CTRL_SYS_ON_DISP_ON);
}

// 2. 全亮功能
void ET6934_All_On(ET6934_Handle_t *dev) { ET6934_Fill_RAM(dev, 0xFF); }

// 3. 全灭功能
void ET6934_All_Off(ET6934_Handle_t *dev) { ET6934_Fill_RAM(dev, 0x00); }
