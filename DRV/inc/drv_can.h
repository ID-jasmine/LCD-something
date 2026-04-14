#ifndef __DRV_CAN_H
#define __DRV_CAN_H

#include <stdint.h>

void DRV_CAN_Init(void);
void DRV_CAN_Monitor_Task(void);
void DRV_CAN_Send_0x220(void);

extern volatile float engine_water_temp;
extern volatile uint8_t can_fault_count;
extern volatile uint16_t can_fault_codes[32];

#endif