#ifndef __BSP_ET6934_H
#define __BSP_ET6934_H

#include "bsp_iic.h"

/* ------------------- ET6934M28 宏定义区 ------------------- */
// 1. 器件 IIC 通信地址
#define ET6934_ADDR_FIXED           0x84    // ET6934M28 固定的器件写地址 [cite: 164]

// 2. 寄存器地址
#define ET6934_REG_DATA_START       0x00    // 显存起始地址 (0x00~0x0F 对应 GRID1~GRID16) [cite: 218, 220]
#define ET6934_REG_MODE_1           0x10    // 显示模式寄存器1：设置恒流和亮度 [cite: 225]
#define ET6934_REG_MODE_2           0x11    // 显示模式寄存器2：设置扫描行数 [cite: 225]
#define ET6934_REG_CTRL             0x12    // 状态控制寄存器：控制工作状态和开关显示 [cite: 227]

// 3. 常用配置参数定义
// 状态控制 (0x12)
#define ET6934_CTRL_SYS_ON_DISP_OFF 0x01    // 工作模式(bit0=1)，关闭显示(bit1=0) [cite: 227]
#define ET6934_CTRL_SYS_ON_DISP_ON  0x03    // 工作模式(bit0=1)，打开显示(bit1=1) [cite: 227]

// 模式1：亮度配置 (0x10)
#define ET6934_MODE1_MAX_BRIGHT     0x0F    // SEG输出70mA，亮度最大(1111) [cite: 225]

// 模式2：扫描行数配置 (0x11)
#define ET6934_MODE2_16_ROWS        0x0F    // 有效 GRID 扫描行数为16行(1111) [cite: 225]

/* ------------------- 数据结构与函数区 ------------------- */

// 定义 ET6934 屏幕句柄
typedef struct {
    IIC_Handle_t* iic;      // 绑定的底层 IIC 句柄指针
    uint8_t dev_addr;       // 设备的 IIC 从机地址
} ET6934_Handle_t;

// 函数声明
void ET6934_Init(ET6934_Handle_t* dev, IIC_Handle_t* iic_handle, uint8_t addr);
void ET6934_All_On(ET6934_Handle_t* dev);
void ET6934_All_Off(ET6934_Handle_t* dev);
//单次写寄存器
void ET6934_Write_Reg(ET6934_Handle_t* dev, uint8_t reg_addr, uint8_t data);

void ET6934_Refresh_RAM(ET6934_Handle_t* dev, uint8_t* pBuffer);


#endif
