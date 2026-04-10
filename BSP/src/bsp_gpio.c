#include "bsp_gpio.h"

#include "app_vehicle.h"
#include "bsp_can.h"
#include "drv_iic.h"
#include "gpio.h"

void BSP_GPIO_init(void) {
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    stc_gpio_cfg_t stcGpioCfg;

    // 输出IO,禁用开漏，即推挽输出
    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enDrv = GpioDrvH;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enCtrlMode = GpioFastIO;

    Gpio_Init(GpioPortC, GpioPin4, &stcGpioCfg); // 子电源控制

    // 输入IO,关闭内部上下拉
    stcGpioCfg.enDir = GpioDirIn;
    stcGpioCfg.enDrv = GpioDrvH;
    stcGpioCfg.enPu = GpioPuDisable;
    stcGpioCfg.enPd = GpioPdDisable;

    Gpio_Init(GpioPortB, GpioPin0, &stcGpioCfg); //! 电门锁，上拉
    Gpio_Init(GpioPortC, GpioPin0, &stcGpioCfg); //! 触摸按钮，上拉

    Gpio_Init(GpioPortC, GpioPin7, &stcGpioCfg); //! 车速脉冲PS1，上拉
    Gpio_Init(GpioPortC, GpioPin6, &stcGpioCfg); //! 转脉冲PS2，上拉

    Gpio_Init(GpioPortB, GpioPin3, &stcGpioCfg);  // 左转PB3，下拉
    Gpio_Init(GpioPortD, GpioPin2, &stcGpioCfg);  // 右转PD2，下拉
    Gpio_Init(GpioPortB, GpioPin4, &stcGpioCfg);  // 发动机故障PB4，上拉
    Gpio_Init(GpioPortC, GpioPin11, &stcGpioCfg); //! ABS PC11，上拉
    Gpio_Init(GpioPortC, GpioPin12, &stcGpioCfg); //! TCS PC12，上拉

    Gpio_EnableIrq(GpioPortC, GpioPin7, GpioIrqRising); // 启动PC7中断
    Gpio_ClearIrq(GpioPortC, GpioPin7);
    EnableNvic(PORTC_E_IRQn, IrqLevel3, TRUE);
    Gpio_EnableIrq(GpioPortC, GpioPin6, GpioIrqRising); // 启动PC6中断
    Gpio_ClearIrq(GpioPortC, GpioPin6);
    EnableNvic(PORTC_E_IRQn, IrqLevel3, TRUE);
    Gpio_EnableIrq(GpioPortB, GpioPin0, GpioIrqFalling); // 启动PB0中断
    Gpio_ClearIrq(GpioPortB, GpioPin0);
    EnableNvic(PORTB_IRQn, IrqLevel0, TRUE);
}

// 未使用及备用引脚的低功耗（模拟输入）初始化
void BSP_GPIO_Unused_Init(void) {
    // 确保 GPIO 外设时钟已开启（如果放在 BSP_GPIO_init 里面，这句可以省去）
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);

    // ================= 端口 A =================
    // PA0-PA3: NC
    Gpio_SetAnalogMode(GpioPortA, GpioPin0);
    Gpio_SetAnalogMode(GpioPortA, GpioPin1);
    Gpio_SetAnalogMode(GpioPortA, GpioPin2);
    Gpio_SetAnalogMode(GpioPortA, GpioPin3);

    // PA9-PA12: NC
    Gpio_SetAnalogMode(GpioPortA, GpioPin9);
    Gpio_SetAnalogMode(GpioPortA, GpioPin10);
    Gpio_SetAnalogMode(GpioPortA, GpioPin11);
    Gpio_SetAnalogMode(GpioPortA, GpioPin12);

    // PA15: NC
    Gpio_SetAnalogMode(GpioPortA, GpioPin15);

    // ================= 端口 B =================
    // PB5: 机油压力报警 (上拉5V，未使用)
    Gpio_SetAnalogMode(GpioPortB, GpioPin5);

    //    // PB12: 启停灯检测 ADC (ADC 引脚本身就必须配置为模拟输入)
    //    Gpio_SetAnalogMode(GpioPortB, GpioPin12);

    // PB15: 水温信号 ADC (未使用)
    Gpio_SetAnalogMode(GpioPortB, GpioPin15);

    // 注意：PB10, PB11 (EEPROM) 已避开，由你的 EEPROM 驱动接管。

    // ================= 端口 C =================
    // PC1: 未使用的输入 IO
    Gpio_SetAnalogMode(GpioPortC, GpioPin1);

    // PC2, PC3: NC
    Gpio_SetAnalogMode(GpioPortC, GpioPin2);
    Gpio_SetAnalogMode(GpioPortC, GpioPin3);

    // PC8, PC9, PC10: NC
    Gpio_SetAnalogMode(GpioPortC, GpioPin8);
    Gpio_SetAnalogMode(GpioPortC, GpioPin9);
    Gpio_SetAnalogMode(GpioPortC, GpioPin10);

    // PC13: NC
    Gpio_SetAnalogMode(GpioPortC, GpioPin13);

    // 注意：PC14, PC15 (晶振) 已避开，绝对不能动。

    // ================= 端口 D & F =================
    // PD00, PD01 不存在，无需处理。
    // PF00, PF01 (晶振) 已避开。
    // PF11 按要求不动。
}

// GPIOC 外部中断服务函数
void PortC_IRQHandler(void) {
    if (TRUE == Gpio_GetIrqStatus(GpioPortC, GpioPin7)) {
        Gpio_ClearIrq(GpioPortC, GpioPin7);
        speed_pulse_cnt++;
    }
    if (TRUE == Gpio_GetIrqStatus(GpioPortC, GpioPin6)) {
        Gpio_ClearIrq(GpioPortC, GpioPin6);
        rpm_pulse_cnt++;
    }
}

// PB0 外部中断服务函数，复苏
void PortB_IRQHandler(void) {
    if (TRUE == Gpio_GetIrqStatus(GpioPortB, GpioPin0)) {
        Gpio_ClearIrq(GpioPortB, GpioPin0);
    }
}

// 深度休眠前：GPIO 漏电封锁配置
void LPM_GPIO_Sleep_Config(void) {
    // PD1控制静态电流
    Gpio_ClrIO(GpioPortC, GpioPin4);

    // 2.
    // 将所有外部带有上拉电阻，且在断电时会悬空的输入引脚，全部切为模拟输入(Analog)
    // 物理断开内部施密特触发器，将漏电流降至 nA 级
    Gpio_SetAnalogMode(GpioPortC, GpioPin7);  // 车速脉冲
    Gpio_SetAnalogMode(GpioPortC, GpioPin6);  // 转速脉冲
    Gpio_SetAnalogMode(GpioPortB, GpioPin3);  // 左转
    Gpio_SetAnalogMode(GpioPortD, GpioPin2);  // 右转
    Gpio_SetAnalogMode(GpioPortB, GpioPin4);  // 发动机故障灯
    Gpio_SetAnalogMode(GpioPortC, GpioPin11); // ABS
    Gpio_SetAnalogMode(GpioPortC, GpioPin12); // TCS
    Gpio_SetAnalogMode(GpioPortC, GpioPin0);  //! 触摸按钮
    // 未在这个翻译单元初始化的
    Gpio_SetAnalogMode(GpioPortB, GpioPin10); // EEPROM SCL
    Gpio_SetAnalogMode(GpioPortB, GpioPin11); // EEPROM SDA

    Gpio_SetAnalogMode(GpioPortB, GpioPin9); // CANTX
    Gpio_SetAnalogMode(GpioPortB, GpioPin8); // CANRX

    // 注意：PB0 (电门锁) 绝对不能设为模拟输入，必须保持原样以接收唤醒中断！
    // 关闭中断
    Gpio_DisableIrq(GpioPortC, GpioPin7, GpioIrqRising);
    Gpio_DisableIrq(GpioPortC, GpioPin6, GpioIrqRising);
    // 清楚中断标志
    Gpio_ClearIrq(GpioPortC, GpioPin7);
    Gpio_ClearIrq(GpioPortC, GpioPin6);
    // 清除所有挂起的中断，防止误唤醒
    NVIC_ClearPendingIRQ(PORTC_E_IRQn);
    // can
}

// 唤醒后：GPIO 状态恢复
void LPM_GPIO_Wakeup_Config(void) {
    // 最简单粗暴且安全的恢复方式：直接重新调用你原有的初始化函数
    // 这样所有的上下拉状态、数字输入模式都会完全恢复到正常工作状态
    BSP_GPIO_init();
    BSP_CAN_Init();
    DRV_IIC_InitAll();
}
