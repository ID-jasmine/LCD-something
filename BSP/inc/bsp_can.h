#ifndef __BSP_CAN_H__
#define __BSP_CAN_H__

#include "ddl.h"
#include <stdbool.h>


void BSP_CAN_Init(void);

extern volatile float engine_water_temp;
extern volatile uint8_t can_fault_count;
extern volatile uint16_t can_fault_codes[32];

extern void CAN_Monitor_Task(void);
extern void Send_CAN_Msg_0x220(void);

#endif /* __BSP_CAN_H__ */
