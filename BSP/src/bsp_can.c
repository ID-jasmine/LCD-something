#include "bsp_can.h"

#include "can.h"
#include "bsp_gpio.h"
#include "drv_can.h"
#include <stdbool.h>

void BSP_CAN_Init(void) {
    // 1. 开启 CAN 和 GPIO 的外设时钟 (根据你的 sysctrl.h 宏定义调整)
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralCan, TRUE); // 开启CAN时钟

    // ================= GPIO 配置 =================
    BSP_GPIO_CanInit();

    // ================= CAN 波特率与模式配置 =================
    stc_can_init_config_t stcCanInitCfg;

    // 配置为正常工作模式，接收所有数据覆盖旧数据
    stcCanInitCfg.enCanRxBufAll = CanRxNormal;          // 只接收正确数据
    stcCanInitCfg.enCanRxBufMode = CanRxBufOverwritten; // 覆盖旧数据
    stcCanInitCfg.enCanSTBMode = CanSTBPrimaryMode;     // 主模式

    // 警告限制值 (使用默认值即可)
    stcCanInitCfg.stcWarningLimit.CanErrorWarningLimitVal = 96;
    stcCanInitCfg.stcWarningLimit.CanWarningLimitVal = 96;

    // 【关键】配置 500kbps 波特率
    // 注意：这里的具体数值取决于给 CAN 外设分配的系统时钟频率！
    // 假设 CAN 时钟是 16MHz，如果要 500kbps，需要分频和时间段组合。
    // 波特率 = CAN时钟 / (PRESC+1) / (SEG_1+2 + SEG_2+1 )
    // 采样点 (SEG_1+2) / (SEG_1+2 + SEG_2+1) 0.75最佳
    stcCanInitCfg.stcCanBt.PRESC = 2 - 1;  // 预分频
    stcCanInitCfg.stcCanBt.SEG_1 = 12 - 2; // 时间段 1
    stcCanInitCfg.stcCanBt.SEG_2 = 4 - 1;  // 时间段 2
    stcCanInitCfg.stcCanBt.SJW = 2 - 1;    // 再同步补偿宽度

    CAN_Init(&stcCanInitCfg);

    // ================= CAN 硬件过滤器配置 =================
    stc_can_filter_t stcFilter;
    stcFilter.enAcfFormat = CanStdFrames; // 假设故障码是标准帧

    // 配置过滤器 1: 过滤 ID 0x101
    stcFilter.enFilterSel = CanFilterSel1;
    stcFilter.u32CODE = 0x101u;
    stcFilter.u32MASK = 0x000u; // 0比较,1不比较
    CAN_FilterConfig(&stcFilter, TRUE);

    // 配置过滤器 2: 过滤 ID 0x402
    stcFilter.enFilterSel = CanFilterSel2;
    stcFilter.u32CODE = 0x402u;
    stcFilter.u32MASK = 0x000u; // 0比较,1不比较
    CAN_FilterConfig(&stcFilter, TRUE);

    // ================= 中断与启动 =================
    // 开启 CAN 接收中断
    CAN_IrqCmd(CanRxIrqEn, TRUE);
    // 开启 NVIC CAN 中断
    EnableNvic(CAN_IRQn, IrqLevel3, TRUE);
}

bool BSP_CAN_Send(uint32_t id, const uint8_t *data, uint8_t len) {
    stc_can_txframe_t txFrame;

    // 1. 基本参数校验
    if (data == NULL || len > 8) {
        return false;
    }

    // 2. 填充发送帧结构体 (严格按照官方库的命名规则)
    txFrame.enBufferSel = CanPTBSel; // 指定主发送缓冲区
    txFrame.StdID = id;              // 设置标准帧ID

    txFrame.Control_f.IDE = 0;   // 0:标准帧 (Identifier Extension bit)
    txFrame.Control_f.RTR = 0;   // 0:数据帧 (Remote Transmission Request)
    txFrame.Control_f.DLC = len; // 设置数据长度

    // 3. 拷贝有效数据
    for (uint8_t i = 0; i < len; i++) {
        txFrame.Data[i] = data[i];
    }

    // 4. 调用底层库函数触发发送 (两步走)
    // 第一步：检查发送状态 (TXA: 0=空闲, 1=忙/正在发送)
    if (TRUE == CAN_StatusGet(CanTxActive)) {
        return false; // 正在发送，跳过本次发送，防止总线拥塞导致死机
    }

    // 第二步：将帧配置写入硬件发送缓冲区
    CAN_SetFrame(&txFrame);

    // 第三步：触发主发送缓冲区 (PTB) 进行发送
    CAN_TransmitCmd(CanPTBTxCmd);

    return true;
}

// ================= CAN 接收功能 =================
// md,之前的模板代码没用定义中断函数和中断回调
void Can_IRQHandler() {
    // 检查是否是接收中断
    if (TRUE == CAN_IrqFlgGet(CanRxIrqFlg)) {
        stc_can_rxframe_t rxFrame;

        CAN_Receive(&rxFrame);
        CAN_IrqFlgClr(CanRxIrqFlg);

        uint32_t receivedId = rxFrame.StdID;
        DRV_CAN_OnRxFrame(receivedId, rxFrame.Data);

        // 清除可能的错误/仲裁丢失标志，防止中断挂死
        if (TRUE == CAN_IrqFlgGet(CanErrorIrqFlg)) {
            CAN_IrqFlgClr(CanErrorIrqFlg);
        }
        if (TRUE == CAN_IrqFlgGet(CanArbiLostIrqFlg)) {
            CAN_IrqFlgClr(CanArbiLostIrqFlg);
        }
        // 清除接收溢出标志位，防止 10ms 高频报文导致中断风暴
        if (TRUE == CAN_IrqFlgGet(CanRxOverIrqFlg)) {
            CAN_IrqFlgClr(CanRxOverIrqFlg);
        }
    }
}
