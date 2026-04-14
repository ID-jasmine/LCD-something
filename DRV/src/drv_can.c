#include "drv_can.h"

#include "bsp_can.h"
#include "drv_time.h"

#include <stdbool.h>

typedef struct {
    uint32_t msg_id;
    uint32_t timeout_ms;
    uint32_t last_rx_time;
    bool is_online;
    bool has_ever_received;
    bool is_updated;
    void (*timeout_callback)(void);
} CanMsgMonitor_t;

typedef struct {
    CanMsgMonitor_t monitors[2];
    uint8_t raw_data_0x101[8];
    uint8_t raw_data_0x402[8];
    uint8_t fault_rx_idx;
} CAN_Context;

static const uint16_t s_fault_ids[71] = {
    0x0030, 0x0031, 0x0032, 0x0036, 0x0037, 0x0038, 0x0053, 0x0054, 0x0105, 0x0106, 0x0107, 0x0108,
    0x0111, 0x0112, 0x0113, 0x0114, 0x0116, 0x0117, 0x0118, 0x0122, 0x0123, 0x0126, 0x0130, 0x0131,
    0x0132, 0x0133, 0x0134, 0x0136, 0x0137, 0x0138, 0x013A, 0x0201, 0x0261, 0x0262, 0x0301, 0x0322,
    0x0412, 0x0413, 0x0414, 0x0420, 0x0444, 0x0458, 0x0459, 0x0480, 0x0501, 0x0506, 0x0507, 0x0508,
    0x0509, 0x0511, 0x0560, 0x0562, 0x0563, 0x0627, 0x0628, 0x0629, 0x0650, 0x0691, 0x0692, 0x1098,
    0x1099, 0x1507, 0x1508, 0x2177, 0x2178, 0x2187, 0x2188, 0x2232, 0x2270, 0x2271, 0x2300};

static CAN_Context s_can_context = {
    .monitors = {
        {0x101, 2000, 0, false, false, false, 0},
        {0x402, 3000, 0, false, false, false, 0},
    },
    .raw_data_0x101 = {0},
    .raw_data_0x402 = {0},
    .fault_rx_idx = 0,
};

volatile float engine_water_temp = 0.0f;
volatile uint8_t can_fault_count = 0;
volatile uint16_t can_fault_codes[32] = {0};

// 二分查找法验证故障码是否合法
static bool Is_Valid_Fault_Code(uint16_t code) {
    int left = 0;
    int right = 70;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (s_fault_ids[mid] == code) {
            return true;
        }

        if (s_fault_ids[mid] < code) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return false;
}

static void WaterTemp_Timeout_Handle(void) {
    engine_water_temp = 0.0f;
}

static void FaultCode_Timeout_Handle(void) {
    can_fault_count = 0;
}

static int CAN_Device_HW_Init(CAN_Device *dev);
static int CAN_Device_HW_MonitorTask(CAN_Device *dev);
static int CAN_Device_HW_Send220(CAN_Device *dev);
static void CAN_Device_HW_OnRxFrame(CAN_Device *dev, uint32_t received_id, const uint8_t data[8]);

static const CAN_Ops s_can_ops = {
    .init = CAN_Device_HW_Init,
    .monitor_task = CAN_Device_HW_MonitorTask,
    .send_0x220 = CAN_Device_HW_Send220,
    .on_rx_frame = CAN_Device_HW_OnRxFrame,
};

CAN_Device g_can_dev = {
    .ops = &s_can_ops,
    .context = &s_can_context,
};

static CAN_Context *CAN_GetContext(CAN_Device *dev) {
    if (dev == 0 || dev->context == 0) {
        return 0;
    }

    return (CAN_Context *)dev->context;
}

static int CAN_Device_Init(CAN_Device *dev) {
    if (dev == 0 || dev->ops == 0 || dev->ops->init == 0) {
        return -1;
    }

    return dev->ops->init(dev);
}

static int CAN_Device_MonitorTask(CAN_Device *dev) {
    if (dev == 0 || dev->ops == 0 || dev->ops->monitor_task == 0) {
        return -1;
    }

    return dev->ops->monitor_task(dev);
}

static int CAN_Device_Send220(CAN_Device *dev) {
    if (dev == 0 || dev->ops == 0 || dev->ops->send_0x220 == 0) {
        return -1;
    }

    return dev->ops->send_0x220(dev);
}

void DRV_CAN_Init(void) {
    (void)CAN_Device_Init(&g_can_dev);
}

void DRV_CAN_Monitor_Task(void) {
    (void)CAN_Device_MonitorTask(&g_can_dev);
}

void DRV_CAN_Send_0x220(void) {
    (void)CAN_Device_Send220(&g_can_dev);
}

void DRV_CAN_OnRxFrame(uint32_t received_id, const uint8_t data[8]) {
    if (g_can_dev.ops == 0 || g_can_dev.ops->on_rx_frame == 0) {
        return;
    }

    g_can_dev.ops->on_rx_frame(&g_can_dev, received_id, data);
}

static int CAN_Device_HW_Init(CAN_Device *dev) {
    (void)dev;
    BSP_CAN_Init();
    return 0;
}

static int CAN_Device_HW_MonitorTask(CAN_Device *dev) {
    CAN_Context *context = CAN_GetContext(dev);
    uint32_t current_time = DRV_Time_Millis();

    if (context == 0) {
        return -1;
    }

    for (int i = 0; i < (int)(sizeof(context->monitors) / sizeof(context->monitors[0])); i++) {
        CanMsgMonitor_t *monitor = &context->monitors[i];

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

    if (context->monitors[0].is_online && context->monitors[0].is_updated) {
        if ((context->raw_data_0x101[6] & 0x80) == 0) {
            uint16_t raw_water_temp = (uint16_t)((context->raw_data_0x101[4] << 8) | context->raw_data_0x101[5]);
            engine_water_temp = (float)raw_water_temp * 0.1f - 273.0f;
        }

        context->monitors[0].is_updated = false;
    }

    if (context->monitors[1].is_online && context->monitors[1].is_updated) {
        uint8_t current_count = context->raw_data_0x402[3];

        if (current_count != can_fault_count) {
            can_fault_count = current_count;
            context->fault_rx_idx = 0;
        }

        if (can_fault_count > 0) {
            uint16_t code1 = (context->raw_data_0x402[4] << 8) | context->raw_data_0x402[5];
            uint16_t code2 = (context->raw_data_0x402[6] << 8) | context->raw_data_0x402[7];

            if (context->fault_rx_idx < can_fault_count && Is_Valid_Fault_Code(code1)) {
                can_fault_codes[context->fault_rx_idx++] = code1;
            }
            if (context->fault_rx_idx < can_fault_count && Is_Valid_Fault_Code(code2)) {
                can_fault_codes[context->fault_rx_idx++] = code2;
            }
            if (context->fault_rx_idx >= can_fault_count) {
                context->fault_rx_idx = 0;
            }
        }

        context->monitors[1].is_updated = false;
    }

    return 0;
}

static int CAN_Device_HW_Send220(CAN_Device *dev) {
    (void)dev;
    uint8_t tx_data[8] = {0};

    return BSP_CAN_Send(0x220u, tx_data, 8) ? 0 : -1;
}

static void CAN_Device_HW_OnRxFrame(CAN_Device *dev, uint32_t received_id, const uint8_t data[8]) {
    CAN_Context *context = CAN_GetContext(dev);

    if (context == 0 || data == 0) {
        return;
    }

    for (int i = 0; i < (int)(sizeof(context->monitors) / sizeof(context->monitors[0])); i++) {
        if (context->monitors[i].msg_id == received_id) {
            context->monitors[i].last_rx_time = DRV_Time_Millis();
            context->monitors[i].is_online = true;
            context->monitors[i].has_ever_received = true;
            context->monitors[i].is_updated = true;

            uint8_t *target_buf = 0;
            switch (received_id) {
            case 0x101:
                target_buf = context->raw_data_0x101;
                break;
            case 0x402:
                target_buf = context->raw_data_0x402;
                break;
            default:
                break;
            }

            if (target_buf != 0) {
                for (int j = 0; j < 8; j++) {
                    target_buf[j] = data[j];
                }
            }

            break;
        }
    }
}