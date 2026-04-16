#ifndef __DRV_CAN_H
#define __DRV_CAN_H

#include <stdint.h>

typedef struct CAN_Device CAN_Device;

// 虚拟函数表
typedef struct {
	int (*init)(CAN_Device *dev);
	int (*monitor_task)(CAN_Device *dev);
	int (*send_0x220)(CAN_Device *dev);
	void (*handle_rx_frame)(CAN_Device *dev, uint32_t received_id, const uint8_t data[8]);
} CAN_Ops;

struct CAN_Device {
	const CAN_Ops *ops;
	void *context;
};

extern CAN_Device g_can_dev;

void DRV_CAN_Init(void);
void DRV_CAN_Monitor_Task(void);
void DRV_CAN_Send_0x220(void);
void DRV_CAN_HandleRxFrame(uint32_t received_id, const uint8_t data[8]);

extern volatile float engine_water_temp;
extern volatile uint8_t can_fault_count;
extern volatile uint16_t can_fault_codes[32];

#endif
