#include "bsp_iic.h"
#include "hc32l07x.h"

// 内部静态延时
static void iic_delay(volatile uint32_t time) {
    if (time == 0) return;
    while (time-- > 0);
}

// --- 高速寄存器方向操作 ---
static void SDA_Dir_In(IIC_Handle_t* iic) {
	stc_gpio_cfg_t stcGpioCfg;
	stcGpioCfg.enDir = GpioDirIn;
	stcGpioCfg.enPu = GpioPuDisable;
	Gpio_Init(iic->sda_port, iic->sda_pin, &stcGpioCfg);
}

static void SDA_Dir_Out(IIC_Handle_t* iic) {
	stc_gpio_cfg_t stcGpioCfg;
	stcGpioCfg.enDir = GpioDirOut;
	stcGpioCfg.enOD = GpioOdEnable;
	Gpio_Init(iic->sda_port, iic->sda_pin, &stcGpioCfg);
}

// --- 基础底层操作 ---
#define IIC_SCL_H(iic)  Gpio_SetIO(iic->scl_port, iic->scl_pin)
#define IIC_SCL_L(iic)  Gpio_ClrIO(iic->scl_port, iic->scl_pin)
#define IIC_SDA_H(iic)  Gpio_SetIO(iic->sda_port, iic->sda_pin)
#define IIC_SDA_L(iic)  Gpio_ClrIO(iic->sda_port, iic->sda_pin)
#define IIC_SDA_IN(iic) Gpio_GetInputIO(iic->sda_port, iic->sda_pin)

void IIC_GPIO_Init(IIC_Handle_t* iic) {
    stc_gpio_cfg_t stcGpioCfg;
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

	// 初始化为开漏输出,有外部上拉
	stcGpioCfg.enDir = GpioDirOut;
	stcGpioCfg.enDrv = GpioDrvH;
	stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdDisable;
	stcGpioCfg.enOD = GpioOdEnable;

    Gpio_Init(iic->sda_port, iic->sda_pin, &stcGpioCfg);
    Gpio_Init(iic->scl_port, iic->scl_pin, &stcGpioCfg);
}

void IIC_Start(IIC_Handle_t* iic) {
    SDA_Dir_Out(iic);
    IIC_SDA_H(iic);
    IIC_SCL_H(iic);
//    iic_delay(4);
    iic_delay(1);
    IIC_SDA_L(iic);
    iic_delay(1);
    IIC_SCL_L(iic);
}

void IIC_Stop(IIC_Handle_t* iic) {
    SDA_Dir_Out(iic);
    IIC_SCL_L(iic);
    IIC_SDA_L(iic);
    iic_delay(1);
    IIC_SCL_H(iic);
    iic_delay(1);
    IIC_SDA_H(iic);
    iic_delay(1);
}

void IIC_Send(IIC_Handle_t* iic, uint8_t data) {
    uint8_t i;
    SDA_Dir_Out(iic);
    IIC_SCL_L(iic);
    for (i = 0; i < 8; i++) {
        if ((data & 0x80) >> 7)
            IIC_SDA_H(iic);
        else
            IIC_SDA_L(iic);
        data <<= 1;
        iic_delay(1);
        IIC_SCL_H(iic);
        iic_delay(1);
        IIC_SCL_L(iic);
        iic_delay(1);
    }
}

void IIC_Wait_Ack(IIC_Handle_t* iic) {
    uint8_t errTime = 0;
    SDA_Dir_In(iic);
    IIC_SDA_H(iic);
    //iic_delay(2);
    iic_delay(1);
    IIC_SCL_H(iic);
    iic_delay(1);
    while (IIC_SDA_IN(iic)) {
        errTime++;
        if (errTime > 250) {
            IIC_Stop(iic);
            break;
        }
    }
    IIC_SCL_L(iic);
}

uint8_t IIC_ReadByte(IIC_Handle_t* iic, uint8_t ack) {
    uint8_t i, receive = 0;
    SDA_Dir_In(iic);
    for (i = 0; i < 8; i++) {
        IIC_SCL_L(iic);
        iic_delay(1);
        IIC_SCL_H(iic);
        receive <<= 1;
        if (IIC_SDA_IN(iic))
            receive++;
        iic_delay(1);
    }
    SDA_Dir_Out(iic);
    IIC_SCL_L(iic);
    if (!ack)
        IIC_SDA_L(iic); // 发送 ACK
    else
        IIC_SDA_H(iic); // 发送 NACK
    iic_delay(1);
    IIC_SCL_H(iic);
    iic_delay(1);
    IIC_SCL_L(iic);
    return receive;
}
