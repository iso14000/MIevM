disclamer.

 FSI code from original one PIEV MIM Imiev CATL93 upgrade., platform Arduino Due + CAN shield 
 the actual structure of this project comes from "Dala the great" , mod for LEAF battery swap 

 Board used for MIevM, is a "MB CAN STM32" or "MB Filter", also known to fool odometer (for bad reason 99% of time) , it is very cheap and affordable.
 Take care to buy only the stm32f105RBT06. PCB is genarally blue.
 
 FSI is my trigram , my first name is Florent , I add "FSI" each time I modify something to keep track of the mod.
until Version revision is V0.x , it is NOT fully tested (not easy to perform!) actually the car is drivable and range reported
is 254Km (in theory I don't swapped my pack yet), and that's all! Don't start mass prod right now!

 feal free to optimize, change, motify  and never forget to notify 
 
 if you perform a swap as B2C with this code as a starting base ... we , the community, ask you to also share your experience (data, streams, tips tricks code method)
 
 if you are earning money by doing so... without sharing you're a f**g b***d  ..... 


V0.1 first test in the car OK => range is 253Km on combination meter , can be in "ready mode" , and can drive.

V0.2 cleanup from Dala's software (Leaf battery pack upgrade) and SoCx clamping [0..100%] ; 
V0.3 now MIevM is connected on P12V (permanent 12V) , so sleep mode is activated after a delay (20s for instance)
V0.4 added a NMC under temperature recharge inhibition. tested on bench only!
				when temp goes below 5°C Vmax is puched to 4.2V, main effect is to forbid regular charging and regeneration
		, changed "char" to "uint8_t" for clarity. Tested on bench, not "in car" ... we are almost burning in Toulouse :-\


quickly validated on my car, report correctly Soc vs Voltage, range decrease while driving, 



>>DONE : TODO take care of energy by activating the sleep feature.

TODO add a busbutton to force SoC2 vs voltage evaluation. (or anything else)

TODO => add a Peukert law

TODO => add a compilation switch to activate Voltage filter (CMU failure filter)

TODO => not a small piece , add a RS232 to monitor / configure features.... 


![2356-f2dc6ea27cf599fa2192ae4c08f08950](https://github.com/user-attachments/assets/1103d7ac-ac8a-44a7-8370-22248550f207)

