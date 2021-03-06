/*
 * Battery Protection Software for BPS PCB Development
 * Originally Written for BPS_V1 2012
 * Modified for BPS_V2 2015 by Scott Haver
 * WMU Sunseeker 2015
 *
 *  I/O Initialization
 */

// Include files
#include "BPSmain.h"

/*
 * Initialise I/O port directions and states
 * Drive unused pins as outputs to avoid floating inputs
 *
 */
void io_init( void )
{
	/******************************PORT 1**************************************/
	P1OUT = 0x00;		   				                    // Pull pins low
  	P1DIR = LED0 | LED1 | CAN_RSTn | CAN_CSn | P1_UNUSED;	//set to output
  	P1DIR &= ~(BUTTON1 | BUTTON2);
	P1OUT = LED0 | LED1 | CAN_RSTn | CAN_CSn;               //Turn on LEDs
	/*Interrupts Enable*/
	//    P1SEL = BUTTON1 | BUTTON1 | CAN_RX0n | CAN_RX1n;  
    //    P1IE  = BUTTON1 | BUTTON2; // Enable Interrupts 
    //    P1IE  |= CAN_RX0n | CAN_RX1n;
    P1IES = BUTTON1 | BUTTON2 | CAN_RX0n | CAN_RX1n; 		//high to low
    P1IFG = 0x00;   										//Clears all interrupt flags on Port 1
    delay();

	/******************************PORT 2**************************************/
	P2OUT = 0x00;						// Pull pins low
 	P2DIR = CAN_INTn | P2_UNUSED;		//set to output
    /*Interrupts Enable */
    //    P2SEL = CAN_INTn; //Interrupts
	//    P2IE  = CAN_INTn; // Enable Interrupts 
	//    P2SEL |= ADC1_RDY | ADC2_RDY | ADC3_RDY | ADC4_RDY ADC1_RDY | ADC5_RDY | ADC6_RDY | ADC7_RDY;
	//    P2IES |= ADC1_RDY | ADC2_RDY | ADC3_RDY | ADC4_RDY ADC1_RDY | ADC5_RDY | ADC6_RDY | ADC7_RDY; 
	//	  P2IE  |= ADC1_RDY | ADC2_RDY | ADC3_RDY | ADC4_RDY ADC1_RDY | ADC5_RDY | ADC6_RDY | ADC7_RDY;
    P2IFG = 0x00;       				//Clears all interrupt flags on Port 2
    delay();

    /******************************PORT 3**************************************/  
	P3OUT = 0x00; 						// Pull pins low       	
   	P3DIR =  CAN_SIMO | CAN_SCLK | ADC_bus1_CLK | ADC_misc_CLK | ADC_bus1_MOSI | ADC_bus2_MOSI | P3_UNUSED;
   	P3OUT |= CAN_SIMO | CAN_SCLK | ADC_bus1_CLK | ADC_misc_CLK | ADC_bus1_MOSI | ADC_bus2_MOSI;
   	P3DIR &= ~(CAN_SOMI | ADC_bus1_MISO);
    P3SEL = ADC_bus1_CLK | CAN_SIMO | CAN_SOMI | CAN_SCLK | ADC_bus1_MOSI | ADC_bus1_MISO | ADC_misc_CLK | ADC_bus2_MOSI;
    
 	/******************************PORT 4**************************************/ 
	P4OUT = 0x00;						// Pull pins low      	
    P4DIR = ADC_CS1 | ADC_CS2 | ADC_CS3 | ADC_CS4 | ADC_CS4 | ADC_CS5 | ADC_CS6 | ADC_CS7 | ADC_RSTn | P4_UNUSED;
	P4OUT = ADC_CS1 | ADC_CS2 | ADC_CS3 | ADC_CS4 | ADC_CS4 | ADC_CS5 | ADC_CS6 | ADC_CS7 | ADC_RSTn;       	
    delay();
	P4OUT &= ~ADC_RSTn ;       	
    delay();
    delay();
	P4OUT |= ADC_RSTn ;       	
	
    /******************************PORT 5**************************************/    
	P5OUT = 0x00;						// Pull pins low       	
    P5DIR = ADC_misc_MOSI | ADC_bus2_CLK | XT2OUT | P5_UNUSED;	
	P5OUT = ADC_misc_MISO | ADC_bus2_MISO;       	
    P5SEL = XT2IN | XT2OUT | ADC_misc_MOSI | ADC_misc_MISO | ADC_bus2_CLK | ADC_bus2_MISO;   
    
    /******************************PORT 6**************************************/   
	P6OUT = 0x00;						// Pull pins low   
	P6DIR = LED2 | LED3 | LED4 | LED5 | RELAY_BATT | RELAY_ARRAY | RELAY_MCPC | RELAY_MC | P6_UNUSED;
	P6OUT |= LED2 | LED3 | LED4 | LED5; //Turn off active low LEDs by pulling high
    P6SEL = 0x00;
    
    /******************************PORT 7**************************************/ 
	P7OUT = 0x00;						// Pull pins low  
	P7DIR = XT1OUT | RELAY_SCPC | RELAY_SC | EXT_RELAY_SCPC | EXT_RELAY_MCPC | P7_UNUSED;      			
    P7SEL = XT1IN | XT1OUT;
    
	/******************************PORT 8**************************************/ 
	P8OUT = 0x00;						// Pull pins low   
	P8DIR |= LT_CS1 | LT_CS2 | LT_CS3 | P8_UNUSED;
	P8SEL = 0x00;
	
    /******************************PORT 9**************************************/    
	P9OUT = 0x00;						// Pull pins low  
	P9DIR = LT_SCLK_1 | LT_MOSI_2 | LT_SCLK_2 | LT_MOSI_1 | P9_UNUSED;  
	P9OUT = LT_SCLK_1 | LT_MOSI_2 | LT_SCLK_2 | LT_MOSI_1;
	P9SEL = LT_SCLK_1 | LT_MOSI_2 | LT_SCLK_2 | LT_MOSI_1 | LT_MISO_2 | LT_MISO_1;
	
    /******************************PORT 10**************************************/ 
	P10OUT = 0x00;						// Pull pins low   
	P10DIR =  EXT_TX | LT_MOSI_3 | LT_SCLK_3 | P10_UNUSED;
	P10OUT =  EXT_TX | LT_MOSI_3 | LT_SCLK_3;
	P10SEL =  EXT_TX | EXT_RX | LT_MOSI_3 | LT_MISO_3 | LT_SCLK_3;
	
	
 	/******************************PORT 11**************************************/ 
 	P11OUT = 0x00;						// Pull pins low
 	P11DIR = ACLK_TEST | MCLK_TEST | SMCLK_TEST; 	
 	P11OUT = ACLK_TEST | MCLK_TEST | SMCLK_TEST;
 	P11SEL = ACLK_TEST | MCLK_TEST | SMCLK_TEST;

    /******************************PORT J**************************************/ 
	PJOUT = 0x00;
	PJDIR = 0x0F;     					//set to output as per user's guide
        
}


