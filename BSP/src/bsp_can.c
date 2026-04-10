#include "bsp_can.h"

#include "can.h"
#include "gpio.h"
#include "main.h"
#include <stdbool.h>

// 定义全局变量
volatile float engine_water_temp = 0.0f;
volatile uint8_t can_fault_count = 0;
volatile uint16_t can_fault_codes[32] = {0};

typedef struct {
    uint32_t msg_id;
    uint32_t timeout_ms;
    uint32_t last_rx_time;
    bool is_online;
    bool has_ever_received;
    bool is_updated;
    void (*timeout_callback)(void);
} CanMsgMonitor_t;

static const uint16_t stcCanFaultIds[71] = {
    0x0030, 0x0031, 0x0032, 0x0036, 0x0037, 0x0038, 0x0053, 0x0054, 0x0105, 0x0106, 0x0107, 0x0108,
    0x0111, 0x0112, 0x0113, 0x0114, 0x0116, 0x0117, 0x0118, 0x0122, 0x0123, 0x0126, 0x0130, 0x0131,
    0x0132, 0x0133, 0x0134, 0x0136, 0x0137, 0x0138, 0x013A, 0x0201, 0x0261, 0x0262, 0x0301, 0x0322,
    0x0412, 0x0413, 0x0414, 0x0420, 0x0444, 0x0458, 0x0459, 0x0480, 0x0501, 0x0506, 0x0507, 0x0508,
    0x0509, 0x0511, 0x0560, 0x0562, 0x0563, 0x0627, 0x0628, 0x0629, 0x0650, 0x0691, 0x0692, 0x1098,
    0x1099, 0x1507, 0x1508, 0x2177, 0x2178, 0x2187, 0x2188, 0x2232, 0x2270, 0x2271, 0x2300};

// 极速二分查找：检查故障码是否在合法列表中
static bool Is_Valid_Fault_Code(uint16_t code) {
    int left = 0;
    int right = 70; // 数组大小 71 - 1

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (stcCanFaultIds[mid] == code) {
            return true; // 找到了，是合法的
        }

        if (stcCanFaultIds[mid] < code) {
            left = mid + 1; // 往右半区找
        } else {
            right = mid - 1; // 往左半区找
        }
    }
    return false; // 找遍了也没匹配上，属于非法/补零码
}

static void WaterTemp_Timeout_Handle(void) { engine_water_temp = 0.0f; }

static void FaultCode_Timeout_Handle(void) { can_fault_count = 0; }

static uint8_t g_raw_data_0x101[8] = {0};
static uint8_t g_raw_data_0x402[8] = {0};

static CanMsgMonitor_t g_can_monitors[] = {
    {0x101, 2000, 0, false, false, false, WaterTemp_Timeout_Handle},
    {0x402, 3000, 0, false, false, false, FaultCode_Timeout_Handle}};

// 可放在100ms刷新一次数据
void CAN_Monitor_Task(void) {
    uint32_t current_time = Get_SystemMs();

    // 超时监控检测
    for (int i = 0; i < sizeof(g_can_monitors) / sizeof(g_can_monitors[0]); i++) {
        CanMsgMonitor_t *monitor = &g_can_monitors[i];
        // 在系统拿到第一帧报文之前，系统保持沉默/离线观察状态
        if (monitor->has_ever_received) {
            if (current_time - monitor->last_rx_time > monitor->timeout_ms) {
                if (monitor->is_online) {
                    monitor->is_online = false;
                    if (monitor->timeout_callback) {
                        monitor->timeout_callback();
                    }
                }
            } else {
                monitor->is_online = true;
            }
        }
    }

    // 2. 协议解析任务
    // --- 0x101 (Engine Data) ---
    if (g_can_monitors[0].is_online && g_can_monitors[0].is_updated) { // 只有在线且有更新才解析
        if ((g_raw_data_0x101[6] & 0x80) == 0) {
            // --- 水温解析 (WaterTemperature) ---
            // 物理值 = 原始值 * 0.1 - 273
            uint16_t rawWaterTemp = (uint16_t)((g_raw_data_0x101[4] << 8) | g_raw_data_0x101[5]);
            engine_water_temp = (float)rawWaterTemp * 0.1f - 273.0f;
        }

        g_can_monitors[0].is_updated = false;
    } // 若超时，engine_water_temp 已经在回调中清零

    //--- 0x402 (Fault Code) ---
    if (g_can_monitors[1].is_online && g_can_monitors[1].is_updated) {
        // --- 故障码解析 ---
        static uint8_t rx_idx = 0;
        uint8_t current_count = g_raw_data_0x402[3]; // Byte 3: 故障数量

        // 当故障数量发生变化时，立刻重置接收索引
        if (current_count != can_fault_count) {
            can_fault_count = current_count;
            rx_idx = 0;
        }
        if (can_fault_count > 0) {
            // 读取本帧中的2个故障码,忽略H字节，取M和L作为16位
            uint16_t code1 = (g_raw_data_0x402[4] << 8) | g_raw_data_0x402[5];
            uint16_t code2 = (g_raw_data_0x402[6] << 8) | g_raw_data_0x402[7];
            // 只有当故障码在合法数组内时，才剔除无效的补零数据并保存到数组
            if (rx_idx < can_fault_count && Is_Valid_Fault_Code(code1)) {
                can_fault_codes[rx_idx++] = code1;
            }
            if (rx_idx < can_fault_count && Is_Valid_Fault_Code(code2)) {
                can_fault_codes[rx_idx++] = code2;
            }
            // 若接收完了一轮，重置索引，等待 ECU 继续循环发送覆盖刷新
            if (rx_idx >= can_fault_count) {
                rx_idx = 0;
            }
        }

        g_can_monitors[1].is_updated = false;
    } // 若超时，can_fault_count 已经在回调中清零
}

void BSP_CAN_Init(void) {
    // 1. 开启 CAN 和 GPIO 的外设时钟 (根据你的 sysctrl.h 宏定义调整)
    Sysctrl_SetPeripheralGate(SysctrlPeripheralGpio, TRUE);
    Sysctrl_SetPeripheralGate(SysctrlPeripheralCan, TRUE); // 开启CAN时钟

    // ================= GPIO 配置 =================
    // 2. 配置 PA08 为推挽输出，并输出低电平 (Low)，唤醒 TJA1042T
    stc_gpio_cfg_t stcGpioCfg;
    stcGpioCfg.enDir = GpioDirOut;
    stcGpioCfg.enDrv = GpioDrvH;
    stcGpioCfg.enOD = GpioOdDisable;
    stcGpioCfg.enCtrlMode = GpioFastIO;
    Gpio_Init(GpioPortA, GpioPin8, &stcGpioCfg);
    Gpio_ClrIO(GpioPortA, GpioPin8); // 拉低 STB 引脚，使能 CAN 收发器

    Gpio_Init(GpioPortB, GpioPin9, &stcGpioCfg);
    Gpio_SetAfMode(GpioPortB, GpioPin9, GpioAf5); // CAN_TX 功能五

    stcGpioCfg.enDir = GpioDirIn;
    Gpio_Init(GpioPortB, GpioPin8, &stcGpioCfg);
    Gpio_SetAfMode(GpioPortB, GpioPin8, GpioAf3); // CAN_RX 功能三

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

// ================= CAN 发送功能 =================
static bool BSP_CAN_Transmit(uint32_t id, uint8_t *data, uint8_t len) {
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

// 下面是一个发送函数模板
// 组装并发送 0x220 报文 ，调用广播
void Send_CAN_Msg_0x220(void) {
    uint8_t tx_data[8] = {0}; // 初始化全0
    // static uint8_t alive_counter = 0; // 静态变量，保持累加状态

    // 1. 填充 ABS 开关状态 (Data[0] 低 3 位)
    // 防御性编程：如果刚上电还没收到ABS的0x210报文，abs_work_mode会是UNKNOWN(0xFF)，
    // 此时默认发全开(0x00)，防止 ABS 接收到错误状态。
    // uint8_t current_abs = (abs_work_mode == ABS_MODE_UNKNOWN)
    //                           ? ABS_MODE_ALL_ON
    //                           : (uint8_t)abs_work_mode;
    // tx_data[0] = current_abs & 0x07;

    // 2. 填充 TCS 开关状态 (Data[1] 低 2 位)
    // tx_data[1] = target_tcs_state & 0x03;

    // 3. Data[3] ~ Data[6] 矩阵未定义，保留默认的 0x00 即可
    // tx_data[2] = tcs_work_state & 0x03;

    // 4. 计算 AliveCounter 心跳 (0~7循环)
    // alive_counter = (alive_counter + 1) & 0x07; // 等价于 % 8

    // 5. 计算 CheckSum (Byte 0 到 Byte 6 数值和的低 5 bits)
    // uint32_t sum = 0;
    // for (int i = 0; i < 7; i++) {
    //     sum += tx_data[i];
    // }
    // uint8_t checksum = sum & 0x1F; // 0x1F 就是 0001 1111，提取低 5 位

    // 6. 拼装 Byte 7
    // 起始位61是Byte7的Bit5，所以心跳左移5位。校验和在低5位。
    // tx_data[7] = (alive_counter << 5) | checksum;

    // 7. 触发底层的 CAN 发送
    BSP_CAN_Transmit(0x220, tx_data, 8);
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

        for (int i = 0; i < sizeof(g_can_monitors) / sizeof(g_can_monitors[0]); i++) {
            if (g_can_monitors[i].msg_id == receivedId) {
                g_can_monitors[i].last_rx_time = Get_SystemMs();
                g_can_monitors[i].is_online = true;
                g_can_monitors[i].has_ever_received = true;
                g_can_monitors[i].is_updated = true;

                // 拷贝原始数据到 task-level 缓冲区
                uint8_t *target_buf = NULL;
                switch (receivedId) {
                case 0x101:
                    target_buf = g_raw_data_0x101;
                    break;
                case 0x402:
                    target_buf = g_raw_data_0x402;
                    break;
                }
                if (target_buf) {
                    for (int j = 0; j < 8; j++)
                        target_buf[j] = rxFrame.Data[j];
                }
                break;
            }
        }

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
