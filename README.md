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


//TODO take care of energy by activating the sleep feature.
//TODO add a busbutton to force SoC2 vs voltage evaluation. (or anything else)
//TODO => add a Peukert law
//TODO => add a compilation switch to activate Voltage filter (CMU failure filter)
//TODO => not a small piece , add a RS232 to monitor / configure features.... 

![2356-f2dc6ea27cf599fa2192ae4c08f08950](https://github.com/user-attachments/assets/1103d7ac-ac8a-44a7-8370-22248550f207)

