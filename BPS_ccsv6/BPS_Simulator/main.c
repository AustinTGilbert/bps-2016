/*
 * Battery Protection Software for BPS2015 PCB Development
 * Based on previous Sunseeker code.
 * Some functions are based on code originally written by
 * Tritium Pty Ltd. See functional modules for Tritium code.
 *
 * Western Michigan University
 * Sunseeker BPS 2015 Simulation Software
 * Created by Scott Haver November 2015
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <msp430x54xa.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "BPSmain.h"
#include "RS232.h"
#include "ad7739_func.h"
#include "LTC6803.h"
#include "can.h"

#define MAX_TEMP_DISCHARGE 		0x4ED916 		//30 Degree C
#define MAX_TEMP_CHARGE   		0x413A8E		//45 Degree C

#define MAX_CURRENT_DISCHARGE	0xFAE147		//60A    @ 2.45 volts across batt shunt
#define MAX_CURRENT_CHARGE		0x3F7CED		//-31.5A @ .62 volts across batt shunt

//LTC Variables
volatile unsigned char ltc1_errflag = 0x00;		//set if LTC1 error
volatile unsigned char ltc2_errflag = 0x00; 	//set if LTC2 error
volatile unsigned char ltc3_errflag = 0x00; 	//set if LTC3 error
unsigned char ltc_error = 0;					//combined error calc
volatile unsigned char batt1,batt2,batt3;		//stores return value of flag read
volatile unsigned char status_flag = FALSE;		//status flag set on timer B
volatile unsigned char batt_KILL = FALSE;		//set to isolate batt
volatile unsigned char batt_ERR = 0x00 ;		//Cause of batt_KILL
int mode_count = 0;								//used for sequencing
unsigned int ltc1_cv[12];						//holds ltc1 cell volts
unsigned int ltc2_cv[12];						//holds ltc2 cell volts
unsigned int ltc3_cv[12];						//holds ltc3 cell volts

//ADC Temperature Variables
volatile unsigned long temperature_adc[40];		//stores adc temperatures
volatile unsigned long current;					//keeps track of current direction across batt shunt
volatile unsigned char ch;						//used for ch switching of adc
volatile unsigned char dev;						//used for dev switching of
volatile unsigned char i;						//used for counting
volatile unsigned char temp_flag = FALSE;		//used for temp measurement timing

// CAN Communication Variables
volatile unsigned char send_can = FALSE;	//used for CAN transmission timing
volatile unsigned char cancomm_flag = FALSE;	//used for CAN transmission timing

volatile int 	max_v_cell, ltc_max;
volatile long	bat_voltage;
volatile float 	max_voltage;
volatile int 	min_v_cell, ltc_min;
volatile float 	min_voltage;
volatile long	max_temp;
volatile int	max_temp_idx;
volatile unsigned char comms_event_count = 0;
volatile unsigned int can_err_cnt = 0x0000;
volatile int cancheck;
volatile unsigned char 	can_CANINTF, can_FLAGS[3];

volatile int switches_out_dif, switches_dif_save;
volatile int switches_out_new, switches_new;
volatile char can_start_precharge = FALSE;
volatile char precharge_complete = FALSE;

volatile char AC_char3, AC_char2, AC_char1, AC_char0;
volatile long AC_Serial_No;

//PreCharge Variables
unsigned long SIG1;								//measures BATT voltage
unsigned long SIG2;								//measures precharge shunt resistor voltage
unsigned long SIG3;								//measures voltage after MC contactor
volatile char PC_Complete = FALSE;				//TRUE when caps are charged
volatile char MC_Complete = FALSE;				//TRUE when MC contactor is closed
unsigned long compare_sig;						//stores signal comparison value

//RS232 Variables
char command[30];								//stores rs232 commands
char buff[30];									//buff array to hold sprintf string
unsigned char rs232_count = 0;					//counts characters received
char batt_temp[30] = "battery temps\r";			//command to read batt temperatures
char batt_temp_status = 0;						//TRUE if battery temps is sent
char batt_volt[30] = "battery volts\r";			//command to read cell volts
char batt_volt_status = 0;						//TRUE if battery volts is sent
char batt_current[30] ="battery current\r";		//command to read battery current
char batt_current_status = 0;					//TRUE if battery current is sent
float volt;										//variable to store adc volt conv
float temp;										//temperature calculation
int temp1;										//integer value
int temp2;										//tenths place
int temp3;										//hundredths place



/*=================================== **MAIN** =========================================*/
//////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
	unsigned int i;

	enum MODE
	{
		INITIALIZE,
		SELFCHECK,
		BPSREADY,
		ARRAYREADY,
		CANCHECK,
		PRECHARGE,
		NORMALOP,
		SHUTDOWN,
		CHARGE,
		ERRORMODE
	}bpsMODE;

	WDTCTL = WDTPW | WDTHOLD | WDTSSEL__ACLK; 	// Stop watchdog timer to prevent time out reset
	_DINT();     		    					//disables interrupts

	//open all relays
	relay_batt_open;
	relay_array_open;
	relay_mcpc_open;
	relay_mc_open;
	ext_relay_mcpc_open;

	bpsMODE = INITIALIZE;

	io_init();									//enable IO pins
	clock_init();								//Configure HF and LF clocks
	timerB_init();								//init timer B

	BPS2PC_init();								//init RS232
	canspi_init();
	can_init();

	//adc bus1 initializations
	adc_bus1_spi_init();
	adc_bus1_init();
	adc_bus1_selfcal(1);
	adc_bus1_read_convert(0,1);
	adc_bus1_selfcal(2);
	adc_bus1_read_convert(0,2);
	adc_bus1_selfcal(3);
	adc_bus1_read_convert(0,3);
	//adc bus2 initializations
	adc_bus2_spi_init();
	adc_bus2_init();
	adc_bus2_selfcal(1);
	adc_bus2_read_convert(0,1);
	adc_bus2_selfcal(2);
	adc_bus2_read_convert(0,2);
	adc_bus2_selfcal(3);
	adc_bus2_read_convert(0,3);
	//adc misc initializations
	adc_misc_spi_init();
	adc_misc_init();
	adc_misc_selfcal();
	adc_misc_read_convert(0);

	i = 50000;									// SW Delay
	do i--;
	while (i != 0);

	//LTC1 Configure
	LTC1_init();
	LTC1_init();
	ltc1_errflag = 0x00;
	//LTC2 Configure
//	LTC2_init();
//	LTC2_init();
	ltc2_errflag = 0x00;
	//LTC3 Configure
//	LTC3_init();
//	LTC3_init();
	ltc3_errflag = 0x00;
	
    /*Enable Interrupts*/
	UCA3IE |= UCRXIE;						//RS232 receive interrupt
	__bis_SR_register(GIE); 				//enable global interrupts
	__no_operation();						//for compiler
	
 	bpsMODE = SELFCHECK;

	while(TRUE)								//loop forever
	{
		if(bpsMODE == SHUTDOWN)
		{
			//open all relays
			relay_batt_open;
			relay_array_open;
			relay_mcpc_open;
			relay_mc_open;
			ext_relay_mcpc_open;
		}
		if(status_flag)
		{
			status_flag = FALSE;
			mode_count++;
			batt_KILL = FALSE;
			batt_ERR = 0x00;

			if(bpsMODE == ERRORMODE)
			{
				//look for CAN message to end errormode
				bpsMODE = SELFCHECK;

			}
			else if(bpsMODE == SHUTDOWN)
			{
				//open all relays
				relay_batt_open;
				relay_array_open;
				relay_mcpc_open;
				relay_mc_open;
				ext_relay_mcpc_open;
			}
			else									//normal periodic sequence based on mode_count
			{
				//batt1
				batt1 = LTC1_Read_Config();
				batt1 |= LTC1_Read_Flags();
				if(batt1 != 0x00)
				{
					batt_KILL = TRUE;
					batt_ERR = 0x11;
					batt1=0x00;
				}
				//batt2
				batt2 = LTC2_Read_Config();
				batt2 |= LTC2_Read_Flags();
				if(batt2 != 0x00)
				{
					//batt_KILL = TRUE;
					//batt_ERR = 0x12;
					batt2=0x00;
				}
				//batt3
				batt3 = LTC3_Read_Config();
				batt3 |= LTC3_Read_Flags();
				if(batt3 != 0x00)
				{
					//batt_KILL = TRUE;
					//batt_ERR = 0x13;
					batt3=0x00;
				}
				// Periodic Measurements
				switch(mode_count)
				{
					case 1:
					  ltc1_errflag = 0x00;
					  ltc2_errflag = 0x00;
					  ltc3_errflag = 0x00;
					break;
					case 2:
					  LTC1_Start_ADCCV();
					break;
					case 3:
					  batt1 = LTC1_Read_Voltages();
					  if (batt1 != 0x00)
					  {
						ltc1_errflag = TRUE;
					  }
					break;
					case 4:
					  LTC2_Start_ADCCV();
					break;
					case 5:
					  batt2 = LTC2_Read_Voltages();
					  if (batt2 != 0x00)
					  {
						// ltc2_errflag = TRUE;
					  }
					break;
					case 6:
					 LTC3_Start_ADCCV();
					break;
					case 7:
					  batt3 = LTC3_Read_Voltages();
					  if (batt3 != 0x00)
					  {
						  //ltc3_errflag = TRUE;
					  }
					break;
					case 8:
						LTC1_Start_ADCCV();
					break;
					default:
					  mode_count = 0;
					break;
				}	// END Switch mode_count

				ltc_error = ltc1_errflag | ltc2_errflag | ltc3_errflag;

				if (ltc_error != 0x00 && (bpsMODE != SELFCHECK))
				{
					batt_KILL = TRUE; // LTC Error
					batt_ERR = 0x20;
					ltc_error = FALSE;
				}
				if (mode_count == 0x08)
				{
					mode_count = 0;

					switch(bpsMODE)
					{
					case SELFCHECK:
						if(batt_KILL != 0x00)
						{
							batt_KILL = FALSE;									//reset  batt_KILL
							batt_ERR = 0x00;
							LED_NORMALOP_OFF;									//LED NORMALOP OFF
							char i;
							for(i = 0; i < 2; i++)								//re-init ltcs reset flags
							{
								LTC1_init();
								LTC2_init();
								LTC3_init();
								ltc1_errflag = 0x00;
								ltc2_errflag = 0x00;
								ltc3_errflag = 0x00;
							}
							//adc bus1 initializations							//re-init adcs and calibrate
							adc_bus1_spi_init();
							adc_bus1_init();
							adc_bus1_selfcal(1);
							adc_bus1_read_convert(0,1);
							adc_bus1_selfcal(2);
							adc_bus1_read_convert(0,2);
							adc_bus1_selfcal(3);
							adc_bus1_read_convert(0,3);
							//adc bus2 initializations
							adc_bus2_spi_init();
							adc_bus2_init();
							adc_bus2_selfcal(1);
							adc_bus2_read_convert(0,1);
							adc_bus2_selfcal(2);
							adc_bus2_read_convert(0,2);
							adc_bus2_selfcal(3);
							adc_bus2_read_convert(0,3);
							//adc misc initializations
							adc_misc_spi_init();
							adc_misc_init();
							adc_misc_selfcal();
							adc_misc_read_convert(0);

						}
						else
						{
							bpsMODE = BPSREADY;
							LED_ERROR_OFF;
						}
					break;

					case BPSREADY:
						relay_batt_close;										//BPS Power Enabled
						bpsMODE = ARRAYREADY;
						canspi_init();
						can_init();
					break;

					case ARRAYREADY:
						relay_array_close;										//Array Connection Enabled
						bpsMODE = CANCHECK;
						send_can = TRUE;		//Start CAN transmissions
					break;

					case CANCHECK:
						//check for CAN message start PRECHARGE
						bpsMODE = PRECHARGE;
						LED_PC_ON;												//turn PC LED ON
						relay_mcpc_close;										//close MCPC contactor
						ext_relay_mcpc_close;									//close external pc relay to read signals
					break;

					case PRECHARGE:
						//start cont conv for PC signals
						adc_misc_contconv_start(5);								//battery signal 			(SIGNAL 1)
						adc_misc_contconv_start(6);								//precharge resistor signal (SIGNAL 2)
						adc_misc_contconv_start(7);								//motor contactor signal    (SIGNAL 3)

						if(PC_Complete == FALSE)								//if caps are not charged
						{
							SIG1 = adc_misc_read_convert(5);
							//wait for valid reading SIG1 92.4 to 147 V
							// R Divider ~0.01525	--> 0x00E5C1D5 to 0x00906B35
							//
							if (SIG1 > 0x00900)
							{
								SIG2 = adc_misc_read_convert(6);				//get SIG2
								compare_sig = abs(SIG1 - SIG2);					//compare SIG1 & SIG2

								if(compare_sig < 0x040000 && SIG2 > 0x00800)	//check comparison is small and SIG2 is not noise
								{
									PC_Complete = TRUE;
								}
								else
								{
									PC_Complete = FALSE;
								}
							}
						}
						else if(MC_Complete == FALSE && PC_Complete == TRUE)	//only run loop when PC_Complete = TRUE
						{

							SIG1 = adc_misc_read_convert(5);
							if (SIG1 > 0x00900)
							{				//wait for valid reading SIG1
								SIG3 = adc_misc_read_convert(7);				//get SIG3 reading
								compare_sig = abs(SIG1 - SIG3);					//compare SIG1 and SIG2

								if(compare_sig < 0x040000 && SIG3 > 0x00800)	//ensure motor contactor is closed
								{
									MC_Complete = TRUE;
								//PC is complete at this point
									relay_mc_close;				  				//close MC contactor
									ext_relay_mcpc_open;						//open external PC relay
									relay_mcpc_open;			  				//open MCPC contactor
									bpsMODE = NORMALOP;
									LED_PC_OFF;									//turn precharge LED OFF
									// Transmit Precharge CAN Message
									can.address = BP_CAN_BASE + BP_PCDONE;
									can.data.data_u8[7] = 'B';
									can.data.data_u8[6] = 'P';
									can.data.data_u8[5] = 'v';
									can.data.data_u8[4] = '1';
									can.data.data_u32[0] = DEVICE_SERIAL;
									cancheck=can_transmit();
									if(cancheck == 1) can_init();
								}
								else
								{
									MC_Complete = FALSE;
								}
							}
						}
					break;

					case NORMALOP:
						LED_NORMALOP_TOG;
					break;

					case CHARGE:
						//
					break;
					}//end switch(bpsMODE)

				}//end if(mode_count == 0x08)

			}//end else

		}//end if(status_flag)


		//////////////////////////CHECK ADC TEMP LIMITS//////////////////////////////////////////
		if(temp_flag)
		{
			temp_flag = FALSE;

			//start adc battery temp continuous conversions
			for(dev = 1; dev < 4; dev++)								//device {1:3}
			{
				for(ch = 0; ch < 8; ch++)								//channel {0:7}
				{
					adc_bus1_contconv_start(ch,dev);
					adc_bus2_contconv_start(ch,dev);
				}
			}

			//start continuous conversion for shunt measurement
			adc_misc_contconv_start(4);
			//start continous conversion for misc temperatures
			adc_misc_contconv_start(2);
			adc_misc_contconv_start(3);


			///////////////BATTERY TEMPS
			//read adc bus1 device 1 temperatures
			for(i = 1; i < 8; i++)
			{
				temperature_adc[8-i] = adc_bus1_read_convert(i,1);		//store temp {1:7}
			}
			//read adc bus1 device 2 temperatures
			for(i = 1; i < 8; i++)
			{
				temperature_adc[15-i] = adc_bus1_read_convert(i,2);		//store temp {8:14}
			}
			//read adc bus1 device 3 temperatures
			for(i = 1; i < 8; i++)
			{
				temperature_adc[22-i] = adc_bus1_read_convert(i,3);		//store temp {15:21}
			}
			//read adc bus2 device 1 temperatures
			for(i = 1; i < 8; i++)
			{
				temperature_adc[29-i] = adc_bus2_read_convert(i,1);		//store temp {22:28}
			}
			//read adc bus2 device 2 temperatures
			for(i = 1; i < 8; i++)
			{
				temperature_adc[36-i] = adc_bus2_read_convert(i,2);		//store temp {29:35}
			}

			///////////ADDITIONAL TEMPS
			temperature_adc[36] = adc_bus2_read_convert(7,3);			//store inlet temp  {36}
			temperature_adc[37] = adc_bus2_read_convert(6,3);			//store outlet temp {37}

			temperature_adc[38] = adc_misc_read_convert(3);				//store misc temp 	{38}
			temperature_adc[39] = adc_misc_read_convert(2);				//store misc temp 	{39}

			/////////CHECK LIMITS

			//get current direction across batt shunt
			for(i = 5; i > 0; i--)
			{
				current = adc_misc_read_convert(4);						//check multiple times to avoid invalid data
			}

			//check temperature and current limits if discharging
			if(current >= 0x800000)										//adc > 1.25V  (DISCHARGING)
			{
				LED_INIT_ON;
				LED_PC_OFF;

				if(current >= MAX_CURRENT_DISCHARGE)					//over current check
				{
					batt_KILL = TRUE;
					batt_ERR = 0x30;
				}
				else
				{
					for(i = 1; i < 5; i++)								//check temperature cells {1:35}
					{
						if(temperature_adc[i] <= MAX_TEMP_DISCHARGE)	//temp max is 60 degree C discharging
						{
							batt_KILL = TRUE;
							batt_ERR = 0x40;
						}
					}
				}
			}

			//check temperature and current limits if charging
			else if(current < 0x800000)									//adc < 1.25V  (CHARGING)
			{
				LED_INIT_OFF;
				LED_PC_ON;
				if(current <= MAX_CURRENT_CHARGE)						//over current check
				{
					batt_KILL = TRUE;
					batt_ERR = 0x50;
				}
				else
				{
					for(i = 1; i < 5; i++)
					{
						if(temperature_adc[i] <= MAX_TEMP_CHARGE)		//temp max is 45 degree C charging
						{
							batt_KILL = TRUE;
							batt_ERR = 0x60;
						}
					}
				}
			}
		}
	   ////////////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////////////HANDLE CAN//////////////////////////////////////////

		// Handle communications events
		if (cancomm_flag && send_can)
		{
			cancomm_flag = FALSE;

		// Max Cell Voltage and total battery voltage
			max_v_cell = 0;
			ltc_max = 0x0000;
			bat_voltage = 0x00000000;
			for(i=0;i<=11;i++)
			{
				if((ltc1_cv[i])>ltc_max)
				{
					ltc_max=ltc1_cv[i];
					max_v_cell = i;
				}
				bat_voltage += (long) (ltc1_cv[i]-512);
			}

			max_voltage = ((float) (ltc_max-512)) * 0.0015;

		// Min Cell Voltage
			min_v_cell = 0;
			ltc_min = 0x0FFF;
			for(i=0;i<=11;i++)
			{
				if((ltc1_cv[i])<ltc_min)
				{
					ltc_min=ltc1_cv[i];
					min_v_cell = i;
				}
			}

			min_voltage = ((float) (ltc_min-512)) * 0.0015;


		// Max Cell Temperature Already Computed
			max_temp = 0x00000000;
			max_temp_idx = 0x0000;
			for(i = 35; i > 0; i--)
			{
				if(temperature_adc[i]>max_temp)
				{
					max_temp = temperature_adc[i];
					max_temp_idx = i;
				}
			}

		// Transmit CAN message
		// Transmit Max Cell Voltage
			can.address = BP_CAN_BASE + BP_VMAX;
			can.data.data_fp[1] = max_voltage;
			can.data.data_fp[0] = (float) max_v_cell;
			can_transmit();

		// Transmit Min Cell Voltage
			can.address = BP_CAN_BASE + BP_VMIN;
			can.data.data_fp[1] = min_voltage;
			can.data.data_fp[0] = (float) min_v_cell;
			can_transmit();

		// Transmit Max Cell Temperature
			can.address = BP_CAN_BASE + BP_TMAX;
			can.data.data_fp[1] = max_temp;
			can.data.data_fp[0] = (float) max_temp_idx;
			can_transmit();

		// Transmit Shunt Cutrrent
			can.address = BP_CAN_BASE + BP_ISH;
			can.data.data_fp[1] = current;
			can.data.data_fp[0] = (float) max_voltage * 0.0015;
			can_transmit();

		// Transmit our ID frame at a slower rate (every 10 events = 1/second)
			comms_event_count++;
			if(comms_event_count >=10)
			{
				comms_event_count = 0;
				can.address = BP_CAN_BASE;
				can.data.data_u8[7] = 'B';
				can.data.data_u8[6] = 'P';
				can.data.data_u8[5] = 'v';
				can.data.data_u8[4] = '1';
				can.data.data_u32[0] = DEVICE_SERIAL;
				cancheck = can_transmit();
				if(cancheck == 1) can_init();
			}

			can_flag_check();
		}  // End periodic communications

		  // Check for CAN packet reception
		if((P1IN & CAN_INTn) == 0x00)
		{
		// IRQ flag is set, so run the receive routine to either get the message, or the error
			can_receive();
		// Check the status
		// Modification: case based updating of actual current and velocity added
		// - messages received at 5 times per second 16/(2*5) = 1.6 sec smoothing
			if(can.status == CAN_OK)
			{
				LED_ERROR_OFF;
				switch(can.address)
				{
				case DC_CAN_BASE + DC_SWITCH:
					switches_out_dif  = can.data.data_u16[3];
					switches_dif_save = can.data.data_u16[2];
					switches_out_new  = can.data.data_u16[1];
					switches_new      = can.data.data_u16[0];
					if((switches_new & SW_IGN_ON) == 0x00)
					{
						if((switches_out_new & 0xFF00) == 0xFF00)
						{
							can_start_precharge = TRUE;
						}
					}
					// Add DC Charge mode here
					break;
				case AC_CAN_BASE + AC_BP_CHARGE:
					AC_char3 = can.data.data_u8[7];
					AC_char2 = can.data.data_u8[6];
					AC_char1 = can.data.data_u8[5];
					AC_char0 = can.data.data_u8[4];
					AC_Serial_No = can.data.data_u32[0];
					// If ACV1 charge mode, else nothing
					break;
				default:
					break;
				}

			}
			else if(can.status == CAN_RTR)
			{
				LED_ERROR_OFF;
				switch(can.address)
				{
					case BP_CAN_BASE:
						can.address = BP_CAN_BASE;
						can.data.data_u8[7] = 'B';
						can.data.data_u8[6] = 'P';
						can.data.data_u8[5] = 'v';
						can.data.data_u8[4] = '1';
						can.data.data_u32[0] = DEVICE_SERIAL;
						can_transmit();
						break;
					case BP_CAN_BASE + BP_VMAX:
						can.data.data_fp[1] = max_voltage;
						can.data.data_fp[0] = (float) max_v_cell;
						can_transmit();
						break;
					case BP_CAN_BASE + BP_VMIN:
						can.data.data_fp[1] = min_voltage;
						can.data.data_fp[0] = (float) min_v_cell;
						can_transmit();
						break;
					case BP_CAN_BASE + BP_TMAX:
						can.data.data_fp[1] = max_temp;
						can.data.data_fp[0] = (float) max_temp_idx;
						can_transmit();
						break;
					case BP_CAN_BASE + BP_ISH:
						can.data.data_fp[1] = current;
						can.data.data_fp[0] = (float) max_voltage * 0.0015;
						can_transmit();
						break;
					case BP_CAN_BASE + BP_PCDONE:
						if(bpsMODE == NORMALOP)
						{
							can.data.data_u8[7] = 'B';
							can.data.data_u8[6] = 'P';
							can.data.data_u8[5] = 'v';
							can.data.data_u8[4] = '1';
							can.data.data_u32[0] = DEVICE_SERIAL;
						}
						else
						{
							can.data.data_u8[7] = 0x00;
							can.data.data_u8[6] = 0x00;
							can.data.data_u8[5] = 0x00;
							can.data.data_u8[4] = 0x00;
							can.data.data_u32[0] = DEVICE_SERIAL;
						}
						can_transmit();
						break;
				}
			}
			else if(can.status == CAN_ERROR)
			{
				LED_ERROR_TOG;
				can_err_cnt++;
			}
		}


		/////////////////////////////////////////HANDLE RS232//////////////////////////////////////////

		if(batt_temp_status)											//cell temperatures
		{
			batt_temp_status = 0;
			BPS2PC_puts("\nBATTERY TEMPERATURES:");
			BPS2PC_puts("MAX Discharge Temp = 60 Degree C");
			BPS2PC_puts("MAX Charge Temp = 45 Degree C\n");
			for(i = 1; i < 40; i++)
			{
				volt = 2.5*(temperature_adc[i] / 16777215.0);
				temp = 116.0 - (111.39*volt);
				temp1 = (int)temp;
				temp2 = (int)(temp * 10) % 10;
				temp3 = (int)(temp * 100) %10;

				sprintf(buff,"Temp %d = %d.%d%d Degree C",i,temp1,temp2,temp3);
				BPS2PC_puts(buff);
			}
		}

		else if(batt_volt_status)										//cell voltages
		{

			batt_volt_status = 0;
			BPS2PC_puts("\nBATTERY CELL VOLTAGES:");
			BPS2PC_puts("MAX CELL VOLTAGE 4.176 V");
			BPS2PC_puts("MIN CELL VOLTAGE 2.808 V\n");
			for(i = 0; i < 12; i++)
			{
				ltc1_cv[i] = get_cell_voltage(i,1);						//cell i ltc1
				ltc2_cv[i] = get_cell_voltage(i,2);						//cell i ltc2
				ltc3_cv[i] = get_cell_voltage(i,3);						//cell i ltc3

				ltc1_cv[i] = (ltc1_cv[i] - 512);
				ltc2_cv[i] = (ltc2_cv[i] - 512);
				ltc3_cv[i] = (ltc3_cv[i] - 512);
			}
			for(i = 0; i < 12; i++)
			{
				temp = ltc1_cv[i] / 666.66;								//conversion to volts
				temp1 = (int)temp;										//integer value
				temp2 = (int)(temp * 10) % 10;							//tenths place
				temp3 = (int)(temp * 100) %10;							//hundredths place

				sprintf(buff, "Cell %d = %d.%d%d Volts",i,temp1,temp2,temp3);
				BPS2PC_puts(buff);
			}
			for(i = 0; i < 12; i++)
			{
				temp = ltc2_cv[i] / 666.66;
				temp1 = (int)temp;
				temp2 = (int)(temp * 10) % 10;
				temp3 = (int)(temp * 100) %10;

				sprintf(buff, "Cell %d = %d.%d%d Volts",i+12,temp1,temp2,temp3);
				BPS2PC_puts(buff);
			}
			for(i = 0; i < 12; i++)
			{
				temp = ltc3_cv[i] / 666.66;
				temp1 = (int)temp;
				temp2 = (int)(temp * 10) % 10;
				temp3 = (int)(temp * 100) %10;

				sprintf(buff, "Cell %d = %d.%d%d Volts",i + 24,temp1,temp2,temp3);
				BPS2PC_puts(buff);
			}

		}

		else if(batt_current_status)								//battery current
		{
			batt_current_status = 0;

			BPS2PC_puts("\nBATTERY CURRENT:");
			BPS2PC_puts("MAX CURRENT DISCHARGE 60A");
			BPS2PC_puts("MAX CURRENT CHARGE -31.5A\n");

			temp = 2.5*(current / 16777215.0);
			temp = 50*(temp - 1.25);
			temp1 = (int)temp;
			temp2 = abs((int)(temp * 10) % 10);
			temp3 = abs((int)(temp * 100) %10);

			sprintf(buff, "Battery Current = %d.%d%d Amps",temp1,temp2,temp3);
			BPS2PC_puts(buff);
		}

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//handle batt_KILL error

		if(batt_KILL)
		{
			LED_ERROR_ON;
			//open all relays
			relay_batt_open;
			relay_array_open;
			relay_mcpc_open;
			relay_mc_open;
			ext_relay_mcpc_open;

			PC_Complete = FALSE;
			MC_Complete = FALSE;

			P6OUT |= batt_ERR>>4;

			LED_NORMALOP_OFF;

			bpsMODE = SELFCHECK;
		}
		else
		{
			LED_ERROR_OFF;
		}


	} //end while(TRUE)

}
/*================================ ** END MAIN** =========================================*/
////////////////////////////////////////////////////////////////////////////////////////////

/*
* Initialise Timer B
*	- Provides timer tick timebase at 100 Hz
*/
void timerB_init( void )
{
  TBCTL = CNTL_0 | TBSSEL_1 | ID_3 | TBCLR;		// ACLK/8, clear TBR
  TBCCR0 = (ACLK_RATE/8/TICK_RATE);				// Set timer to count to this value = TICK_RATE overflow
  TBCCTL0 = CCIE;								// Enable CCR0 interrrupt
  TBCTL |= MC_1;								// Set timer to 'up' count mode
}

/*
* Timer B CCR0 Interrupt Service Routine
*	- Interrupts on Timer B CCR0 match at 10Hz
*	- Sets Time_Flag variable
*/
/*
* GNU interropt symantics
* interrupt(TIMERB0_VECTOR) timer_b0(void)
*/
#pragma vector = TIMERB0_VECTOR
__interrupt void timer_b0(void)
{
	static unsigned int status_count = LTC_STATUS_COUNT;
	static unsigned int temp_count = LTC_STATUS_COUNT/2;
	static unsigned int cancomm_count = CAN_COMMS_COUNT;

	status_count--;
	temp_count--;
    if( status_count == 0 )
    {
    	P6OUT ^= LED2|LED3|LED4|LED5;
    	status_count = LTC_STATUS_COUNT;
    	status_flag = TRUE;
    }
    if(temp_count == 0)
    {
    	temp_count = LTC_STATUS_COUNT/2;
    	temp_flag = TRUE;
    }
    if(send_can) cancomm_count--;
    if( cancomm_count == 0 )
    {
        cancomm_count = CAN_COMMS_COUNT;
        cancomm_flag = TRUE;
    }

}

//RS232 Interrupt
#pragma vector = USCI_A3_VECTOR
__interrupt void USCI_A3_ISR(void)
{
	UCA3IV = 0x00;

	command[rs232_count] = BPS2PC_getchar();		//get character
	BPS2PC_putchar(command[rs232_count]);			//put character

	if(command[rs232_count] == 0x0D)				//if return
	{
		rs232_count = 0x00;							//reset counter
		if(strcmp(batt_temp,command) == 0) batt_temp_status = 1;
		else if(strcmp(batt_volt,command) == 0) batt_volt_status = 1;
		else if(strcmp(batt_current, command) == 0) batt_current_status = 1;
		else
		{
			batt_temp_status = 0;
			batt_volt_status = 0;
			batt_current_status = 0;
		}

		for(i = 0; i < 31; i++)
		{
			command[i] = 0;							//reset command
		}
		BPS2PC_putchar(0x0A);						//new line
		BPS2PC_putchar(0x0D);						//return
	}
	else if(command[rs232_count] != 0x7F)			//if not backspace
	{
		rs232_count++;
	}
	else
	{
		rs232_count--;
	}

}


