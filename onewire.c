#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "global.h"
#include "onewire.h"

/////////////////////////////////////////////////////////////////////////////
// Values saves in EEP ROM 
EE_RAM_t __attribute__((section (".eeprom"))) eeprom =
{
		150,  // t_conv ms
		1280, // uint8_t  t_reset_tx; ~ 480us
		1063, // uint8_t  t_reset_rx; ~ 400us
		200,  // t_reset_delay        ~ 75us
		80,   // uint8_t  t_write_low ~ 30us
		180,  // uint8_t  t_write_slot~ 70us
		30,   // uint8_t  t_read_samp;
		100,  // uint8_t  t_read_slot;
};

/////////////////////////////////////////////////////////////////////////////
// Global variables
DS_t DS;

void therm_init(void)
{
	uint8_t i;
	for (i = 0; i < 9; i++)
		DS.scratchpad[i] = 0;
	for (i = 0; i < 8; i++)
		DS.dev_id[i] = 0;
	
	TRIG_HIGH(TRIG_RESET_PIN);
	TRIG_HIGH(TRIG_READ_PIN);
	TRIG_HIGH(TRIG_READ_BYTE);
	TRIG_HIGH(TRIG_WRITE_BYTE);
	TRIG_HIGH(TRIG_CMD);
	
	therm_set_pin(0);
	
	DS.t_conv       = eeprom_read_word(&eeprom.t_conv);
	DS.t_reset_tx   = eeprom_read_word(&eeprom.t_reset_tx);
	DS.t_reset_rx   = eeprom_read_word(&eeprom.t_reset_rx);
	DS.t_reset_delay= eeprom_read_word(&eeprom.t_reset_delay);
	DS.t_write_low  = eeprom_read_byte(&eeprom.t_write_low);
	DS.t_write_slot = eeprom_read_byte(&eeprom.t_write_slot);
	DS.t_read_samp  = eeprom_read_byte(&eeprom.t_read_samp);
	DS.t_read_slot  = eeprom_read_byte(&eeprom.t_read_slot);
}

void therm_set_pin(uint8_t newPin)
{
	/*
	pin number to port/pin mapping for Arduino 328 hardware
	# PORT PIN
	0  D   2
	1  D   3 
	2  D   4   
	3  D   5 
	4  D   6 
	5  D   7 
	6  B   0
	7  B   1
	8  B   3
	9  B   4
	
	*/
	if(newPin < 6)
	{		
		DS.therm_port    = PORTD;
		DS.therm_ddr     = DDRD;
		DS.therm_pin_reg = PIND;
		DS.therm_pin     = newPin + 2;
		rprintf("Setting pin to PORTD %d\n",DS.therm_pin);
	}
	else
	{
		DS.therm_port    = PORTB;
		DS.therm_ddr     = DDRB;
		DS.therm_pin_reg = PINB;
		DS.therm_pin     = newPin - 6;
		rprintf("Setting pin to PORTB %d",DS.therm_pin);
	}
	rprintf("DS.therm_ddr  = %d\n",DS.therm_ddr);
	rprintf("DS.therm_port = %d\n",DS.therm_port);
	rprintf("DS.therm_pin  = %d\n",DS.therm_pin);
}
void therm_delay(uint16_t delay)
{
	while (delay--)
		asm volatile("nop");
}

void therm_print_timing()
{// Print timing currently used for comms
	rprintf("\n1Wire Timing\n");
	rprintf("01 t_conv       : %d\n",DS.t_conv);
	rprintf("02 t_reset_tx   : %d\n",DS.t_reset_tx);
	rprintf("03 t_reset_rx   : %d\n",DS.t_reset_rx);
	rprintf("04 t_reset_delay: %d\n",DS.t_reset_delay);	
	rprintf("05 t_write_low  : %d\n",DS.t_write_low);
	rprintf("06 t_write_slot : %d\n",DS.t_write_slot);
	rprintf("07 t_read_samp  : %d\n",DS.t_read_samp);
	rprintf("08 t_read_slot  : %d\n",DS.t_read_slot);
}

void therm_set_timing(uint8_t time, uint16_t interval)
{// Set new time intervals for 1wire comms
	cli();
	switch (time)
	{
	case 1:
		eeprom_write_word(&eeprom.t_conv, interval);
		break;
	case 2:
		eeprom_write_word(&eeprom.t_reset_tx, interval);
		break;
	case 3:
		eeprom_write_word(&eeprom.t_reset_rx, interval);
		break;
	case 4:
		eeprom_write_word(&eeprom.t_reset_delay, interval);
		break;
	case 5:
		eeprom_write_byte(&eeprom.t_write_low, interval);
		break;
	case 6:
		eeprom_write_byte(&eeprom.t_write_slot, interval);
		break;
	case 7:
		eeprom_write_byte(&eeprom.t_read_samp, interval);
		break;
	case 8:
		eeprom_write_byte(&eeprom.t_read_slot, interval);
		break;
	default:
		break;
	}
	therm_init();
	sei();
}

void therm_test(void)
{
	rprintf("DS.therm_ddr  = %d\n",DS.therm_ddr);
	rprintf("DDRD          = %d\n",DDRD);
	rprintf("DS.therm_port = %d\n",DS.therm_port);
	rprintf("PORTD         = %d\n",PORTD);
	rprintf("DS.therm_pin  = %d\n",DS.therm_pin);
}
/////////////////////////////////////////////////////////////////////////////
// Low level reset, bit read/write  functions
uint8_t therm_reset(){
	/*
	All communication with the DS18B20 begins with an initialization sequence that consists of a reset pulse
	from the master followed by a presence pulse from the DS18B20. This is illustrated in Figure 13. When
	the DS18B20 sends the presence pulse in response to the reset, it is indicating to the master that it is on
	the bus and ready to operate.
	During the initialization sequence the bus master transmits (TX) the reset pulse by pulling the 1-Wire bus
	low for a minimum of 480µs. The bus master then releases the bus and goes into receive mode (RX).
	When the bus is released, the 5kΩ pullup resistor pulls the 1-Wire bus high. When the DS18B20 detects
	this rising edge, it waits 15µs to 60µs and then transmits a presence pulse by pulling the 1-Wire bus low
	for 60µs to 240µs.
	
	Figure 13 Reset pulse
	Vpulup _______                        ____	                     ___________________
	              \                      /    \                     /
	               \                    /      \                   /
	                \__________________/        \_________________/

	                                               |x| <- master samples
	              |<------ 480us ----->|<-15us->|<---- 60-240us --->|
	                                   |<------------ 480us ----------->|

	*/
	uint8_t presence_pulse;
	TRIG_LOW(TRIG_RESET_PIN);

	// During the initialization sequence the bus master transmits (TX) the reset pulse by pulling the 1-Wire bus
	// low for a minimum of 480µs. The bus master then releases the bus and goes into receive mode (RX).
	//THERM_OUTPUT_MODE(DS.therm_ddr, DS.therm_pin);	
	THERM_OUTPUT_MODE(DDRD, DS.therm_pin);	
	THERM_LOW(PORTD, DS.therm_pin);
	therm_delay(DS.t_reset_tx); //480 us

	THERM_INPUT_MODE(DDRD, DS.therm_pin);	
	therm_delay(DS.t_reset_delay);
	
	TRIG_HIGH(TRIG_RESET_PIN);	
	presence_pulse = therm_read_n_times(10,5);
	TRIG_LOW(TRIG_RESET_PIN);	
	therm_delay(DS.t_reset_rx); //480 - 75 us	
	TRIG_HIGH(TRIG_RESET_PIN);	
	return (presence_pulse == 0);
}

void therm_write_bit(uint8_t bit)
{// Perform single bit write operation 

	/*
	WRITE TIME SLOTS
	There are two types of write time slots: “Write 1” time slots and “Write 0” time slots. The bus master
	uses a Write 1 time slot to write a logic 1 to the DS18B20 and a Write 0 time slot to write a logic 0 to the
	DS18B20. All write time slots must be a minimum of 60µs in duration with a minimum of a 1µs recovery
	time between individual write slots. Both types of write time slots are initiated by the master pulling the
	1-Wire bus low (see Figure 14).
	To generate a Write 1 time slot, after pulling the 1-Wire bus low, the bus master must release the 1-Wire
	bus within 15µs. When the bus is released, the 5kΩ pullup resistor will pull the bus high. To generate a
	Write 0 time slot, after pulling the 1-Wire bus low, the bus master must continue to hold the bus low for
	the duration of the time slot (at least 60µs).
	The DS18B20 samples the 1-Wire bus during a window that lasts from 15µs to 60µs after the master
	initiates the write time slot. If the bus is high during the sampling window, a 1 is written to the DS18B20.
	If the line is low, a 0 is written to the DS18B20.

			  |     Master Write 0 slot   |       |       Master Write 1 slot   
			  |<- 60us < Tx "0" < 120us ->|       |<- 1us < Trec < inf
	Vpulup ___                               ____	    ___________________________________
	          |                             /    |     /
	          |                            /     |    /
	          |___________________________/      |___/
	                |      DS Samples     |            |      DS Samples     |
	                | min    typ      max |            | min    typ      max |
	                |---------------------|            |---------------------|
	          | 15us|<- 15us ->|<- 30us ->|      | 15us|<- 15us ->|<- 30us ->| 
	          |<-DS.t_write_low->|               |<-DS.t_write_low->|

	*/	
	TRIG_LOW(TRIG_WRITE_PIN);
	THERM_LOW(PORTD, DS.therm_pin);
	THERM_OUTPUT_MODE(DDRD, DS.therm_pin);	
	therm_delay(DS.t_write_low);
	//If we want to write 1, release the line (if not will keep low)	
	if (bit)
		THERM_INPUT_MODE(DDRD, DS.therm_pin);	
	//Wait for 60uS and release the line
	therm_delay(DS.t_write_slot);
	TRIG_HIGH(TRIG_WRITE_PIN);
	THERM_INPUT_MODE(DDRD, DS.therm_pin);
}

uint8_t therm_read_bit(void)
{//	perform single bit read operation 
	
	/*
	The DS18B20 can only transmit data to the master when the master issues read time slots. Therefore, the
	master must generate read time slots immediately after issuing a Read Scratchpad [BEh] or Read Power
	Supply [B4h] command, so that the DS18B20 can provide the requested data. In addition, the master can
	generate read time slots after issuing Convert T [44h] or Recall E2 [B8h] commands to find out the status
	of the operation as explained in the DS18B20 Function Commands section.
	All read time slots must be a minimum of 60µs in duration with a minimum of a 1µs recovery time
	between slots. A read time slot is initiated by the master device pulling the 1-Wire bus low for a
	minimum of 1µs and then releasing the bus (see Figure 14). After the master initiates the read time slot,
	the DS18B20 will begin transmitting a 1 or 0 on bus. The DS18B20 transmits a 1 by leaving the bus high
	and transmits a 0 by pulling the bus low. When transmitting a 0, the DS18B20 will release the bus by the
	end of the time slot, and the bus will be pulled back to its high idle state by the pullup resister. Output 
	data from the DS18B20 is valid for 15µs after the falling edge that initiated the read time slot. Therefore,
	the master must release the bus and then sample the bus state within 15µs from the start of the slot.
	Figure 15 illustrates that the sum of TINIT, TRC, and TSAMPLE must be less than 15µs for a read time slot.
	Figure 16 shows that system timing margin is maximized by keeping TINIT and TRC as short as possible
	and by locating the master sample time during read time slots towards the end of the 15µs period.

			  |     Master Read  0 slot   |       |       Master Read 1 slot   
			  |<- 60us < Tx "0" < 120us ->|       |<- 1us < Trec < inf
	Vpulup ===                               ___=	    ___________________________________
	          |                             /    |     /
	          |                            /     |    /
	          |==_________________________/      |=__/
				
			        |-|<- Master sample	               |-|<- Master sample	
	          |<-15us->|<------45us------>|      |<-15us->|

	Figure 15. Detailed Master Read 1 Timing

	Vpulup ====                                     ______________________________________
	          |                               ____/ 
	          |                           ___/
	          |                       ___/    
	GND       |====================__/     
	          
	          |<-- T_init > 1us -->|<-- T_rc -->| Master Sample  |
	          |<---------------------- 15us -------------------->|

	*/

	uint8_t bit = 0;
	
	TRIG_LOW(TRIG_READ_PIN);
	
	//Pull line low for 1uS
	THERM_LOW(PORTD, DS.therm_pin);
	THERM_OUTPUT_MODE(DDRD, DS.therm_pin);
	therm_delay(1);
	//Release line and wait for 14uS
	THERM_INPUT_MODE(DDRD, DS.therm_pin);
	therm_delay(DS.t_read_samp);
	
	TRIG_HIGH(TRIG_READ_PIN);
	//if (PIND & (1 << DS.therm_pin))
	//	bit = 1;
	bit = therm_read_n_times(4,2);
	TRIG_LOW(TRIG_READ_PIN);
	
	//bit = therm_read_n_times(1,0);	
	//Wait for 45uS to end and return read value
	therm_delay(DS.t_read_slot);
	TRIG_HIGH(TRIG_READ_PIN);
	
	return bit;
}

uint8_t therm_read_n_times(uint8_t n, uint8_t threshold)
{// Take multiple read samples to improve reliability. 

	uint8_t val=0;
	while(n--)	
		val += READ_PIN(PIND, DS.therm_pin);
	
	return (val > threshold);
}

/////////////////////////////////////////////////////////////////////////////
// Low level reset, byte read/write functions
uint8_t therm_read_byte(void)
{// Perform single byte read operation 
	uint8_t i = 8, n = 0;
	cli();
	TRIG_LOW(TRIG_READ_BYTE);
	while (i--)
	{
		//Shift one position right and store read value
		n >>= 1;
		n |= (therm_read_bit() << 7);
	}
	TRIG_HIGH(TRIG_READ_BYTE);
	sei();
	return n;
}

void therm_write_byte(uint8_t byte)
{// Perform single byte write operation 
	uint8_t i = 8;
	cli();
	TRIG_LOW(TRIG_WRITE_BYTE);
	while (i--)
	{
		//Write actual bit and shift one position right to make the next bit ready
		therm_write_bit(byte & 1);
		byte >>= 1;
	}
	TRIG_HIGH(TRIG_WRITE_BYTE);
	sei();
}

/////////////////////////////////////////////////////////////////////////
void therm_print_scratchpad()
{// Print content of scratchpad after reading from device
	uint8_t i;
	rprintfStr("[");
	for (i = 0; i < 9; i++)
	{
		rprintfNum(10, 3, 0, ' ', DS.scratchpad[i]);
		if (i !=8)
			rprintfStr(",");
	}
	rprintfStr("]");
}
void therm_print_devID()
{// Print device id in decimal format
	uint8_t i;
	rprintfStr("\"");
	for (i = 0; i < 8; i++)
	{
		rprintf("%d",DS.dev_id[i]);
		if (i==7)
			rprintf("\"");
		else
			rprintf(".");
	}
}

/////////////////////////////////////////////////////////////////////////
uint8_t therm_load_devID(uint8_t therm_pin, uint8_t devNum)
{// Loads device ID saved in eeprom.

	uint8_t no_error = 0, crc[1], i = 0;
	uint16_t address_sum = 0;
	for (i = 0; i < 8; i++)
	{
		DS.dev_id[i] = eeprom_read_byte(&eeprom.rom[therm_pin][devNum][i]);
		address_sum += DS.dev_id[i];
	}
	if (address_sum > 0)
	{
		no_error = therm_crc_is_OK(DS.dev_id, crc, 7);		
	}
	return no_error;
}

void therm_save_devID(uint8_t devNum)
{// Save device id to eeprom
	uint8_t i;
	cli();
	for (i = 0; i < 8; i++)	
		eeprom_write_byte(&eeprom.rom[DS.therm_pin][devNum][i], DS.dev_id[i]);	
	sei();
}

void therm_set_devID(uint8_t *dev_id)
{// sets the id by copying dav_id to DS.dev_id.  Useful as api in external functions
 // that don't have access to DS structure.
	uint8_t i;
	for (i = 0; i < 9; i++){
		DS.dev_id[i] = dev_id[i];		
	}
}

void therm_send_devID()
{
	uint8_t i = 0;
	if (DS.dev_id[0] == 0)
	{
		therm_write_byte(THERM_CMD_SKIPROM);
	}
	else
	{
		therm_write_byte(THERM_CMD_MATCHROM);
		for (i = 0; i < 8; i++)
			therm_write_byte(DS.dev_id[i]);
	}
}

uint8_t therm_read_devID()
{// Reads device id if only a single device is connected to the network.
	uint8_t no_error = 0, i = 0,crc[1];
	crc[0] = 0;
	therm_reset();	
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_reset();
	therm_write_byte(THERM_CMD_READROM);
	for (i = 0; i < 8; i++)
		DS.dev_id[i] = therm_read_byte();

	no_error = therm_crc_is_OK(DS.dev_id, crc, 7);

	return no_error;
}

void therm_start_measurement()
{// Send start conversion command to all devices
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(THERM_CMD_CONVERTTEMP);
}
uint8_t therm_read_scratchpad(uint8_t numOfbytes){
	uint8_t i = 0, crc[1], no_error;
	therm_send_devID();
	therm_write_byte(THERM_CMD_RSCRATCHPAD);
	for (i = 0; i < numOfbytes; i++)
		DS.scratchpad[i] = therm_read_byte();
	crc[0] = 0;
	no_error = therm_crc_is_OK(DS.scratchpad, crc, numOfbytes - 1);
	return no_error;
}
uint8_t therm_read_temperature(uint8_t devNum, int16_t *temperature)
{
	uint8_t no_error = 0;
	temperature[0] = 999;
	temperature[1] = 9999;
	if (therm_load_devID(DS.therm_pin, devNum))
	{
		therm_reset();
		therm_start_measurement();
		_delay_ms(750);
		therm_reset();
		no_error = therm_read_scratchpad(9);
		//therm_print_scratchpad(s);rprintfCRLF();
		if (no_error)
		{
			temperature[0] = (int16_t) (((DS.scratchpad[1] << 8)
					| (DS.scratchpad[0])) >> 1);
			if (temperature[0] < 0)
				temperature[1] = (int16_t) (10000
						- (int16_t) (DS.scratchpad[6])
								* THERM_DECIMAL_STEPS_12BIT);
			else
				temperature[1] = (int16_t) (10000
						- (int16_t) (DS.scratchpad[6])
								* THERM_DECIMAL_STEPS_12BIT);

		}
	}
	return no_error;
}
uint8_t therm_read_result(int16_t *temperature)
{
	uint8_t no_error = 1;
	temperature[0] = 999;
	temperature[1] = 9999;

	therm_reset();

	if (no_error)
	{
		if(DS.dev_id[0] == DS18S20)
		{
			no_error = therm_read_scratchpad(9);
			//temperature[0] = (int16_t) (((DS.scratchpad[1] << 8) | (DS.scratchpad[0])) >> 1);
				temperature[0] = ((int16_t)((DS.scratchpad[1]<<8) | DS.scratchpad[0]));
				if (DS.scratchpad[1] == 255)
				{
					temperature[0] = (temperature[0] >> 1);
					if (DS.scratchpad[6] >= 12)
					{
						temperature[1] = (int16_t) (DS.scratchpad[6] * 625) - (int16_t) (7500);
					}
					else
					{
						temperature[0] += 1;
						temperature[1] = (int16_t) (2500) + (int16_t) (DS.scratchpad[6] * 625);
						if(temperature[0] == 0)
							rprintf("-");
					}
				}
				else
				{
					temperature[0] = (temperature[0] >> 1);
					if (DS.scratchpad[6] > 12)
					{
						temperature[0] -= 1;
						if(DS.scratchpad[0] == 0)
						{
							temperature[1] = (DS.scratchpad[6]) * 625 - 7500;
							temperature[0] = 0;
							if (DS.scratchpad[6] > 12)
								rprintf("-");
						}
						// else if ((DS.scratchpad[0] == 2) & (DS.scratchpad[6] == 12))
					    // temperature[0] += 1;
						else
							temperature[1] = (int16_t) (17500) - (int16_t) (DS.scratchpad[6] * 625);
					}
					else
						temperature[1] = (int16_t) (7500) - (int16_t) (DS.scratchpad[6] * 625);
				}
			}
			else if(DS.dev_id[0] == DS18B20)
				{
					no_error = therm_read_scratchpad(9);
					temperature[0] = (int16_t) (((DS.scratchpad[1] << 8) | (DS.scratchpad[0])) >> 4);
					temperature[1] = (int16_t) (((DS.scratchpad[1] << 8) | (DS.scratchpad[0])) & 15)*THERM_DECIMAL_STEPS_12BIT;
				}
			else if (DS.dev_id[0] == DS2438)
			{
				if (get_ds2438_temperature())
				{
					temperature[0] = (int16_t) (DS.scratchpad[2]);
					temperature[1] = (int16_t) (DS.scratchpad[1]);
				}
			}

			else				{
					rprintf("\"DEV_NOT_FOUND\"");
				}
	}
	else
		//rprintf("CRC ERROR");
		;

	rprintf("%d.",temperature[0]);
	rprintfNum(10, 4, 0, '0', temperature[1]);
	return no_error;
}
uint8_t therm_computeCRC8(uint8_t inData, uint8_t seed)
{
	uint8_t bitsLeft;
	uint8_t temp;

	for (bitsLeft = 8; bitsLeft > 0; bitsLeft--)
	{
		temp = ((seed ^ inData) & 0x01);
		if (temp == 0)
		{
			seed >>= 1;
		}
		else
		{
			seed ^= 0x18;
			seed >>= 1;
			seed |= 0x80;
		}
		inData >>= 1;
	}
	return seed;
}
uint8_t therm_crc_is_OK(uint8_t *scratchpad, uint8_t *crc, uint8_t numOfBytes)
{
	uint8_t i = 0, id_sum=0;
	crc[0] = 0;
	for (i = 0; i < numOfBytes; i++)
	{
		crc[0]  = therm_computeCRC8(scratchpad[i], crc[0]);
		id_sum += scratchpad[i];
	}
	return ((scratchpad[numOfBytes] == crc[0]) && (id_sum > 0));
}

//////////////////////////////////////////////////////////////
// DS2438
//
void recal_memory_page(uint8_t page)
{
	uint8_t i = 0, crc[1],no_error, numOfbytes = 9;
	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	_delay_ms(1);
	therm_write_byte(0xb8);
	therm_write_byte(page);

	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	_delay_ms(1);
	therm_write_byte(0xbe);
	therm_write_byte(page);
	for (i = 0; i < 9; i++)
		DS.scratchpad[i] = therm_read_byte();
		crc[0] = 0;
	no_error = therm_crc_is_OK(DS.scratchpad, crc, numOfbytes - 1);
}
void test_ds2438()
{
	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(THERM_CMD_CONVERTTEMP);
	_delay_ms(25);

	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(THERM_CMD_CONVERT_VOLTAGE);

	_delay_ms(25);

	recal_memory_page(0);
	therm_print_scratchpad();
	therm_reset();
}

uint8_t get_ds2438_temperature(void)
{
	uint8_t i = 0, crc[1], no_error = -1, numOfbytes = 9;
	no_error =  1;//therm_load_devID(devNum);
	if (no_error)
	{
		therm_reset();
		therm_send_devID();

		_delay_ms(1);
		therm_write_byte(0xb8);
		therm_write_byte(0);

		therm_reset();
		therm_send_devID();
		_delay_ms(1);
		therm_write_byte(0xbe);
		therm_write_byte(0);
		for (i = 0; i < numOfbytes; i++)
			DS.scratchpad[i] = therm_read_byte();
		crc[0] = 0;
		no_error = therm_crc_is_OK(DS.scratchpad, crc, numOfbytes - 1);
	}
	return no_error;
}

void write_to_page(uint8_t page, uint8_t val)
{
	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(0x4e);
	therm_write_byte(page);
	therm_write_byte(val);
	therm_write_byte(val+1);
	therm_write_byte(val+3);
	therm_write_byte(val+4);
	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(0x48);
	therm_write_byte(page);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Search algorithm
// the 1-wire search algo taken from:
// http://www.maxim-ic.com/appnotes.cfm/appnote_number/187
//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//

uint8_t therm_find_first_dev()
{
   // reset the search state
   DS.last_discrepancy = 0;
   DS.last_device_flag = FALSE;
   DS.last_family_discrepancy = 0;

   return therm_run_tree_search();
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
uint8_t therm_find_next_dev()
{
   // leave the search state alone
   return therm_run_tree_search();
}

//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
void therm_search_init(void)
{
	uint8_t i = 0;
	for (i = 0; i < 8; i++)
		DS.dev_id[i] = 0;
}
uint8_t therm_run_tree_search()
{
   unsigned char id_bit_number;
   unsigned char last_zero, rom_byte_number, search_result;
   unsigned char id_bit, cmp_id_bit;
   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number   = 1;
   last_zero       = 0;
   rom_byte_number = 0;
   rom_byte_mask   = 1;
   search_result   = 0;
   DS.crc8         = 0;

   // if the last call was not the last one
   if (!DS.last_device_flag)
   {
	  // rprintf("\nLastDeviceFlag = 0\n");	
      // 1-Wire reset
      if (therm_reset() == 0)
      {
		 rprintf("therm_reset()\n");	
         // reset the search
         DS.last_discrepancy = 0;
         DS.last_device_flag = FALSE;
         DS.last_family_discrepancy = 0;
         return FALSE;
	  }
	  else
		 //rprintf("sending search command\n");	

      // issue the search command
	  therm_write_byte(THERM_CMD_SEARCHROM);

      // loop to do the search
      do
      {
         // read a bit and its complement
         id_bit     = therm_read_bit();
         cmp_id_bit = therm_read_bit();

         // check for no devices on 1-wire
         if ((id_bit == 1) && (cmp_id_bit == 1))
            break;
         else
         {
            // all devices coupled have 0 or 1
            if (id_bit != cmp_id_bit)
               search_direction = id_bit;  // bit write value for search
            else
            {
               // if this discrepancy if before the Last Discrepancy
               // on a previous next then pick the same as last time
               if (id_bit_number < DS.last_discrepancy)
                  search_direction = ((DS.dev_id[rom_byte_number] & rom_byte_mask) > 0);
               else
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == DS.last_discrepancy);

               // if 0 was picked then record its position in LastZero
               if (search_direction == 0)
               {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9)
                     DS.last_family_discrepancy = last_zero;
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
            {
                //DS.ROM_NO[rom_byte_number] |= rom_byte_mask;
			 	DS.dev_id[rom_byte_number] |= rom_byte_mask;
			}
            else
			{
              	//DS.ROM_NO[rom_byte_number] &= ~rom_byte_mask;
			  	DS.dev_id[rom_byte_number] &= ~rom_byte_mask;
			}
            // serial number search direction write bit
            //OWWriteBit(search_direction);
			therm_write_bit(search_direction);

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0)
            {
                docrc8(DS.dev_id[rom_byte_number]);  // accumulate the CRC
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7
	  // end of do-while loop 
	  /////////////////////////////////////////////////////////////////////////////////
	  
      // if the search was successful then
      if (!((id_bit_number < 65) || (DS.crc8 != 0)))
      {
         // search successful so set DS.last_discrepancy,DS.last_device_flag,search_result
         DS.last_discrepancy = last_zero;

         // check for last device
         if (DS.last_discrepancy == 0)
            DS.last_device_flag = TRUE;

         search_result = TRUE;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !DS.dev_id[0])
   {
      DS.last_discrepancy       = 0;
      DS.last_device_flag        = FALSE;
      DS.last_family_discrepancy = 0;
      search_result         = FALSE;
   }

   return search_result;
}

//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
uint8_t therm_verify_tree_search()
{
   unsigned char rom_backup[8];
   unsigned char i,rslt,ld_backup,ldf_backup,lfd_backup;

   // keep a backup copy of the current state
   for (i = 0; i < 8; i++)
      rom_backup[i] = DS.dev_id[i];
   ld_backup = DS.last_discrepancy;
   ldf_backup = DS.last_device_flag;
   lfd_backup = DS.last_family_discrepancy;

   // set search to find the same device
   DS.last_discrepancy = 64;
   DS.last_device_flag = FALSE;

   if (therm_run_tree_search())
   {
      // check if same device found
      rslt = TRUE;
      for (i = 0; i < 8; i++)
      {
         if (rom_backup[i] != DS.dev_id[i])
         {
            rslt = FALSE;
            break;
         }
      }
   }
   else
     rslt = FALSE;

   // restore the search state
   for (i = 0; i < 8; i++)
      DS.dev_id[i] = rom_backup[i];
   DS.last_discrepancy = ld_backup;
   DS.last_device_flag = ldf_backup;
   DS.last_family_discrepancy = lfd_backup;

   // return the result of the verify
   return rslt;
}

void therm_test_func(void)
{
	uint8_t i;	
	for (i = 0; i < 8; i++)
		{
			rprintf("%d",DS.dev_id[i]);
			if (i==7)
				rprintf("\"");
			else
				rprintf(".");
		}
	rprintfCRLF();
}

// TEST BUILD
static unsigned char dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current
// global 'crc8' value.
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
   // See Application Note 27
   // TEST BUILD
   DS.crc8 = dscrc_table[DS.crc8 ^ value];
   return DS.crc8;
}
