#ifndef can_bridge_firmware_h
#define can_bridge_firmware_h

#include "main.h"
#include "can.h"

void one_second_ping( void );

void can_imiev_handler (uint8_t can_bus, CAN_FRAME *frame);
float storeSoC2(uint8_t VoltMin);

void  StateMachine(); //state machine management
void 	LedStateMachine(uint8_t); // state machine for Led manadgment
void 	LedStateMachineUnderSampled(uint8_t);

#define LED_FORCE_OFF 255
#endif
