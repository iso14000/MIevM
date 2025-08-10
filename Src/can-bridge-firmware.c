//disclamer.
// FSI code from original one PIEV MIM Imiev CATL93 upgrade., platform Arduino Due + CAN shield 
// the actual structure of this project comes from "Dala the great" , mod for LEAF battery swap 
// FSI is my trigram , my first name is Florent , I add "FSI" each time I modify something to keep track of the mod.
//until Version revision is V0.x , it is NOT fully tested (not easy to perform!) actually the car is drivable and range reported
//is 254Km (in theory I don't swapped my pack yet), and that's all! Don't start mass prod right now!
// feal free to optimize, change, motify  and never forget to notify 
// if you perform a swap as B2C with this code as a starting base ... we , the community, ask you to also share your experience (data, streams, tips tricks code method)
// if you are earning money by doing so... without sharing you're a f**g b***d  ..... 


//V0.1 first test in the car OK => range is 253Km on combination meter , can be in "ready mode" , and can drive.
//V0.2 cleanup from Dala's software (Leaf battery pack upgrade) and SoCx clamping [0..100%] ; 
//quickly valdated on my car, report correctly Soc vs Voltage, range decrease while driving,
//V0.3 now MIevM is connected on P12V (permanent 12V) , so sleep mode is activated after a delay (20s for instance)
//V0.4 added a NMC under temperature recharge inhibition. tested on bench only!
//				when temp goes below 5°C Vmax is puched to 4.2V, main effect is to forbid regular charging and regeneration
//			, changed "char" to "uint8_t" for clarity.


//TODO add a busbutton to force SoC2 vs voltage evaluation. (or anything else)
//TODO => add a Peukert law
//TODO => add a compilation switch to activate Voltage filter (CMU failure filter)
//TODO => not a small piece , add a RS232 to monitor / configure features.... 


#include "can.h"
#include "can-bridge-firmware.h"
#include <stdio.h>
#include <string.h>

//FSI local defines 
#define mLEDON	LED_Port->BSRR = (uint32_t)LED << 16u; //reset pin (Led On) positive pulse of 40ns
#define mLEDOFF	LED_Port->BSRR = LED; //set pin (Led OFF)

// FSI => some usefull defines
#define CATL93_CAPACITY 90  //Battery Capacity in Ah
#define VOLTAGE_OFFSET 210	// offset used for char to volt convertion 
#define VOLT_TO_CHAR(value) (value*100-VOLTAGE_OFFSET) //macro to convert a voltage to a char /!\ check for overflow at compilation.
#define TIME_REST_FOR_SOC_VOLTAGE_EVAL_MS 60000 // expresed  by x10ms ( 10 min  actually)
#define CURRENT_FOR_SOC_VOLTAGE_EVAL 2.0 //in ampere
#define VOLTAGE_LOWEST	2.75
#define VOLTAGE_HIGHEST 4.2

//long presTime;             //Time in milliseconds
//float step;                //Current interval
//long prevTime = millis();  //Time since last current calculation

//sequencer state machine variables
unsigned int OldEventTime1ms=0; //should be same type as uwTick
unsigned int NextEventDelay1ms=0;
enum{
  InitialState=0,
	StartUp,
	NormalRun,
	NoRegen,
}SystemState;
uint8_t		CurrentState=InitialState;

unsigned int OldLedEventTime1ms=0; //should be same type as uwTick
unsigned int NextLedEventDelay1ms=0;
enum{
	LedOffState=0,
	LedOnState,
	LedBlinkSlow,
	LedBlinkOnce,
	LedBlinkTwice,
	LedBlinkQuick,
	LedExecuteSeq
}LedStates;
uint8_t 	LedState=LedOffState;
uint16_t LedPattern=0;


#define REGEN_INHIBIT	1
#define	NO_ACTION		0
uint8_t CommandFlag=NO_ACTION;

uint8_t TempMinCellArray	[8];

uint8_t TestMinTemp(uint8_t );
#define MIN_TEMP 55 //( 5°C)

void can_imiev_handler (uint8_t can_bus, CAN_FRAME *frame) //FSI handler for Imiev only
{
	static float remAh1 = CATL93_CAPACITY;  //Capacity remaining in battery based on coulomb counting
	static float remAh2 = CATL93_CAPACITY;  //Capacity remaining in battery based either on battery voltage or coulomb counting
	float 	SoC1;        //State of Charge based on coulomb counting "guess O metter"
	float 	SoC2;        //State of Charge based either on battery voltage or coulomb counting "barregraph"
	static uint8_t	 j = 0;      //Counter for valid data
	static uint8_t flag = 0;   //power up flag
	static long centiSec=0;  //Time when  amps between -CURRENT_FOR_SOC_VOLTAGE_EVAL and +CURRENT_FOR_SOC_VOLTAGE_EVAL
														// based on 0x373 lessage recurrence.
	static uint8_t	IndexTemp=0;
	//static char vMax= 0;
	static uint8_t vMin =VOLT_TO_CHAR(2.76) ; // and powerup , first voltage is initialized to the lowest possible
																				 // it should be refreshed to a better value immediatly afterward.

	//uint8_t blocked = 0; // temp local variable
	
	switch (frame->ID)
        {
					
				case 0x373: //10ms reccurence
					/*
					373	Battery data ; Transmitted every 10ms. Data bits:
					D0: Battery Cell Maximum Voltage (V): (D0 + 210) / 10 (OBDZero) <= FSI not sure it is the good formula....
					D1: Battery Cell Minimum Voltage (V): (D1 + 210) / 10 (OBDZero)
					D2-D3: Battery pack amps (A) (D2 * 256 + D3 - 32768)/100 <= FSI not sure it is the good formula....
					D4-D5: Battery pack voltage (V) (D4 * 256 + D5) / 10
					// reference https://github.com/KommyKT/i-miev-obd2/blob/master/README.md#236
					
								*/
				
				/* here we gonna acquire Voltage and Current reported by BMU to EV_ECU and Combination Meter to :
						add or remove energy (in coulomb counting way) to variables remAh1 and remAh1,
				
						
					if ( frame->data[0] > 65 &&  frame->data[0] < 210) 
					vMax =  frame->data[0]; }  */ //FSI => vMax not used

				
				//only valid vMin Value can be used, last good one is used if the actual is out of specs
						if ( frame->data[1] > VOLT_TO_CHAR(VOLTAGE_LOWEST) &&  frame->data[1] < VOLT_TO_CHAR(VOLTAGE_HIGHEST)) 
							{ vMin =  frame->data[1]; }
							
	// FSI => the function below is executed one time only after board powerup.
	// the trick is : if board is connected to ACC (12V switched by EV-control-relay) SoC2 could be evaluated 
	// with voltage lookup table ... BUT , nothing says that battery did have enough rest to have a acceptable value.
	// after a drift for 130Kmh on the highway , drawing 100A , if I stop the car and restart, the board restart on SoC2 wrong value (too low)
	// so I'll prefer to never stop the board by keeping it +P12V....
	
							if (flag == 0)
							{                      //YES!  we gonna initialize some few things
								if (j++ >= 20) {              //wait 20 0x373 frames until CMU is providing good data
									SoC2=storeSoC2(vMin);				//get battery SoC based on voltage (without current!)
									remAh1 = (SoC2*CATL93_CAPACITY)/100;  //calculate the initial remaining Ah in the battery based on voltage
									remAh2 = remAh1;  //calculate the start remaining Ah in the battery based on voltage
									flag = 1;
								}
							}
							
						uint8_t ah = frame->data[2];  //store current bytes and calculate battery current
						uint8_t al = frame->data[3];
						float amps = (ah * 256 + al - 32700) / 100.0;  //Use 32700 not 32768 32700 is the calibrated value
						float Ah = amps / 360000.0;                         //Amphours to or from the battery in the 0.01 sec between 0x373 frames
						remAh1 = remAh1 + Ah;             //update remaining amp hours based on Ah in or out of battery
						remAh2 = remAh2 + Ah;             //update remaining amp hours based on Ah in or out of battery
							
							
						if (amps > -CURRENT_FOR_SOC_VOLTAGE_EVAL && amps < CURRENT_FOR_SOC_VOLTAGE_EVAL) 
								{  //store centiSec elapsed while the amps are between value...
									centiSec ++;
								} else {
									centiSec = 0;
								}
//___________________FSI /!\ just for test ... to test regen inhibition.
						if (CommandFlag==REGEN_INHIBIT) 
						{
							frame->data[0] = VOLT_TO_CHAR(VOLTAGE_HIGHEST);
						};
//___________________ END //FSI /!\ just for test ... to test regen inhibition.
						//frame->data[0] = vMax; // FSI => vMax never used elsewhere. 
						//frame->data[1] = vMin; // FSI => keep original value to handle a potential failure
				break;
				
				
				case 0x374: //100ms reccurence 
								/*
					Battery SOC
					Transmitted every 100ms. Data bits:
					D0: State of charge (%): (D0 - 10) / 2 (OBDZero)
					D1: State of charge (%): (D1 - 10) / 2
					D4: Cell Maximum temperature (oC): D4 - 50 (OBDZero)
					D5: Cell Minimum temperature (oC): D5 - 50 (OBDZero)
					D6: Battery 100% capacity (Ah): D6 / 2 (OBDZero)
			*/
				// reference https://github.com/KommyKT/i-miev-obd2/blob/master/README.md#236
					
					frame->data[6] = 2 * CATL93_CAPACITY;  //modify to 2 x the capacity

					SoC1= (100.0 * remAh1 )/ CATL93_CAPACITY;
				
					if (SoC1>100) //clamp SoC1 value for real life
					{
						SoC1=100;
						remAh1=CATL93_CAPACITY;
					}
					if (SoC1<0) SoC1=0;//clamp SoC1 value for real life
					
					if (centiSec > TIME_REST_FOR_SOC_VOLTAGE_EVAL_MS)
						{             //when battery current has been low for long enough  for battery voltage to settle
							SoC2=storeSoC2(vMin);                        //gets SoC based on lowest cell voltage
							remAh2 = (SoC2  * CATL93_CAPACITY)/100;  //correct remaining capacity based on voltage
						} else 
							{      //current has not been low, long enough to use the SoC based on voltage
								SoC2 = (100.0 * remAh2) / CATL93_CAPACITY;    //correct SoC based on the remaining Ah
								if (SoC2>100) //clamp SoC2 value for real life
									{
										SoC2=100;
										remAh2=CATL93_CAPACITY;
									}
									if (SoC2<0) SoC2=0; //clamp SoC2 value for real life
							}
					//modify data coming from BMU
					frame->data[0] =(uint8_t) 2 * SoC1 + 10;
					frame->data[1] =(uint8_t) 2 * SoC2 + 10;
					
					//filling min temperature array 
					TempMinCellArray[IndexTemp++]= frame->data[5];
					if (IndexTemp>7) IndexTemp=0;
					
					// FSI basic test if 	((frame->data[5])>50)	mLEDON else mLEDOFF;
				
				
					//frame->data[0] = 90;// just for test
					//frame->data[1] = 90;// just for test
					//HAL_GPIO_TogglePin(LED_Port,LED); //FSI
				break;
					
				default:
				break;
			}
	
			if (can_bus == 0)
				{
						PushCan(1, CAN_TX, frame); //frame from CAN port 0 => CAN port 1
				}
			else
				{
						PushCan(0, CAN_TX, frame); //frame from CAN port 1 => CAN port 0
				}
}


//this function takes around 20µs in the actual configuration (25Mhz Xtal and PLL engaged)
float storeSoC2(uint8_t VoltMin) {
  if (VoltMin < VOLT_TO_CHAR(2.75)) {
    return 0 ;
  } 
	else if (VoltMin < VOLT_TO_CHAR(3.00)) {
		return ( (4.082 * VoltMin)/100 + 4.082*VOLTAGE_OFFSET/100 -11.2255);
	} 
	else if (VoltMin < VOLT_TO_CHAR(3.47)) {
    return ( (33.497 * VoltMin ) / 100.0 + 33.497*VOLTAGE_OFFSET/100-99.471);
  } 
	else if (VoltMin < VOLT_TO_CHAR(3.60)) {
    return ( (132.143 * VoltMin) / 100.0 +132.143*VOLTAGE_OFFSET/100 - 441.573);
  } 
	else if (VoltMin < VOLT_TO_CHAR(3.72)) {
		return ( (183.199 * VoltMin) / 100.0 +183.199*VOLTAGE_OFFSET/100 - 625.684);
  } 
	else if (VoltMin < VOLT_TO_CHAR(3.81)){
		return ( (89.213 * VoltMin) / 100.0 +89.213*VOLTAGE_OFFSET/100 - 275.962);
  } 
	else if (VoltMin < VOLT_TO_CHAR(3.92)) {
		return ( (131.098 * VoltMin) / 100.0 +131.098*VOLTAGE_OFFSET/100 -  435.5);
  }
	else if (VoltMin < VOLT_TO_CHAR(4.00)) {
		return ( (100.031 * VoltMin) / 100.0 +100.031*VOLTAGE_OFFSET/100 -  313.686);
  } 
	else if (VoltMin < VOLT_TO_CHAR( 4.20)) {
		return ( (135.913 * VoltMin) / 100.0 +135.913*VOLTAGE_OFFSET/100 -  457.106);
   
    
  } 
	else {
    return 113.727;
  }
	
}


void one_second_ping( void )
{
    //FSI=> removed legacy prog, but keep function for futur use     
}

				
void  StateMachine() //state machine management
{
	switch (CurrentState)
		{
			case InitialState :
				if (uwTick-OldEventTime1ms>=NextEventDelay1ms )
					{
				//next event name and timing is :
				CurrentState=NormalRun;
				OldEventTime1ms=uwTick; //should we inhibit IT during this operation?
				NextEventDelay1ms=2000;//2s later
				//action
				LedState=LedBlinkQuick; //during warmup led is blinking quicky
					}
					break;
				//_________________________________
			
			case NormalRun :
				if (uwTick-OldEventTime1ms>=NextEventDelay1ms ){
				//next event name and timing is :
				OldEventTime1ms=uwTick; 
				NextEventDelay1ms=1000;// re enter the state in 1s.
				//action
				LedState=LedBlinkSlow; //
				if (!TestMinTemp(MIN_TEMP)){
						CommandFlag=REGEN_INHIBIT; // no regen allowed
						CurrentState=NoRegen; // go to noregen state
						LedState=LedBlinkOnce;
					}
				}
				break;
				//_________________________________
			case NoRegen :
				if (uwTick-OldEventTime1ms>=NextEventDelay1ms ){
				//next event name and timing is :
				OldEventTime1ms=uwTick; 
				NextEventDelay1ms=1000;//re enter the state in 1s.
				//action
				LedState=LedBlinkOnce; //
				if (TestMinTemp(MIN_TEMP+2)){ /take car of hysteresis
						CommandFlag=NO_ACTION; //  regen allowed
						CurrentState=NormalRun; // go to normal run
						LedState=LedBlinkSlow; //
					}	
				
				}
				break;
		
			default :
			{
				CurrentState=InitialState;
				OldEventTime1ms=uwTick; //should we inhibit IT during this operation?
				NextEventDelay1ms=1000;//1s later
			}
			break;
			//_________________________________
		}
	}
      
void LedStateMachineUnderSampled (uint8_t LedOrder)
{
	static uint8_t TickUnderSAmple=0	; // variable toggled every 256ms based on uwTick 1ms timer
	if ((!TickUnderSAmple)&& (uwTick & (uint32_t)128) ) //L=>H edge detection
	{
		LedStateMachine(LedOrder);
		TickUnderSAmple=1;
	}
	else 
		if ( (TickUnderSAmple) && !(uwTick & (uint32_t)128) )//H=>L edge detection
		{
			LedStateMachine(LedOrder);
			TickUnderSAmple=0;
		}
}
	
void 	LedStateMachine(uint8_t LedOrder) //LED state machine management called ~128ms by LedStateMachineUnderSampled
{
	
	static uint8_t LastLedState =0;
	if (LedOrder==LED_FORCE_OFF)  LedState=LedOffState;
	if (LastLedState!=LedState)  //something change?
	{
			LastLedState=LedState;
			switch (LedState)
				{
						case LedOffState :
						mLEDOFF
						LedPattern=0x0000;
						break;
						
						case LedOnState :
						LedPattern=0xFFFF;
						mLEDON
						break;
						
						case LedBlinkSlow :
							LedPattern=0xF0F0;
						break;
						
						case LedBlinkOnce :
							LedPattern=0x0001;
						break;
						
						case LedBlinkTwice :
							LedPattern=0x000A;
						break;

						
						case LedBlinkQuick :
							LedPattern=0xAAAA;
						break;
						
						default :
						LedPattern=0x0000;
						mLEDOFF
						break;
					}
				}
						
				
			if (LedPattern&0x0001)
			{
				LedPattern=LedPattern>>1;
				LedPattern=LedPattern+0x8000;
				mLEDON;
			}
			else 
			{
				mLEDOFF;
				LedPattern=LedPattern>>1;
			}
						
			
} 

uint8_t TestMinTemp(uint8_t Temperature2Compare)
{
	uint8_t i=0;
	uint8_t AccumulativeFilter=0;
	do 
	{
		if (TempMinCellArray[i++]<=Temperature2Compare)
			AccumulativeFilter++;
		if (AccumulativeFilter>=6)
			return 0; // a cell is critically too cold => no regen requested!
	}while (i<8);
	return 1; // no cells under critical temp 
}

//wip 
//____________________________________end ____________________________________
