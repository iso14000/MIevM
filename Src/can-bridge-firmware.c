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



//TODO add a busbutton to force SoC2 vs voltage evaluation. (or anything else)
//TODO => add a Peukert law
//TODO => add a compilation switch to activate Voltage filter (CMU failure filter)
//TODO => not a small piece , add a RS232 to monitor / configure features.... 


#include "can.h"
#include "can-bridge-firmware.h"
#include <stdio.h>
#include <string.h>


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



void can_imiev_handler (uint8_t can_bus, CAN_FRAME *frame) //FSI handler for Imiev only
{
	static float remAh1 = CATL93_CAPACITY;  //Capacity remaining in battery based on coulomb counting
	static float remAh2 = CATL93_CAPACITY;  //Capacity remaining in battery based either on battery voltage or coulomb counting
	float 	SoC1;        //State of Charge based on coulomb counting
	float 	SoC2;        //State of Charge based either on battery voltage or coulomb counting
	static char	 j = 0;      //Counter for valid data
	static char flag = 0;   //power up flag
	static long centiSec=0;  //Time when  amps between -CURRENT_FOR_SOC_VOLTAGE_EVAL and +CURRENT_FOR_SOC_VOLTAGE_EVAL
														// based on 0x373 lessage recurrence.
	
	//static char vMax= 0;
	static char vMin =VOLT_TO_CHAR(2.76) ; // and powerup , first voltage is initialized to the lowest possible
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
							
						char ah = frame->data[2];  //store current bytes and calculate battery current
						char al = frame->data[3];
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
								SoC2 = (100.0 * remAh2) / CATL93_CAPACITY;    //correct SoC bsed on the remaining Ah
								if (SoC2>100) //clamp SoC2 value for real life
									{
										SoC2=100;
										remAh2=CATL93_CAPACITY;
									}
									if (SoC2<0) SoC2=0; //clamp SoC2 value for real life
							}
					//modify data coming from BMU
					frame->data[0] =(char) 2 * SoC1 + 10;
					frame->data[1] =(char) 2 * SoC2 + 10;
					
					//Can1.sendFrame(incoming);  //send the corrected SoCs and the correct 100% capacity to the ECU
				
				
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
float storeSoC2(char VoltMin) {
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
//____________________________________end ____________________________________
