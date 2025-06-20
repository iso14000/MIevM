#ifndef can_bridge_firmware_h
#define can_bridge_firmware_h

#include "main.h"
#include "can.h"

void one_second_ping( void );

void can_imiev_handler (uint8_t can_bus, CAN_FRAME *frame);
float storeSoC2(char VoltMin);

#endif
