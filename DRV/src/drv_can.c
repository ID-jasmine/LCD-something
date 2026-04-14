#include "drv_can.h"

#include "bsp_can.h"

void DRV_CAN_Init(void) {
    BSP_CAN_Init();
}

void DRV_CAN_Monitor_Task(void) {
    CAN_Monitor_Task();
}

void DRV_CAN_Send_0x220(void) {
    Send_CAN_Msg_0x220();
}