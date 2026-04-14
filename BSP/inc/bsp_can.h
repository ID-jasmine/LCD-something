#ifndef __BSP_CAN_H__
#define __BSP_CAN_H__

#include "ddl.h"
#include <stdbool.h>


void BSP_CAN_Init(void);
bool BSP_CAN_Send(uint32_t id, const uint8_t *data, uint8_t len);

#endif /* __BSP_CAN_H__ */
