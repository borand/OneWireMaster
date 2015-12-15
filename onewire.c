
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "global.h"
#include "onewire.h"

EE_RAM_t __attribute__((section (".eeprom"))) eeprom =
{
		150,  // t_conv ms
		1100, // uint8_t  t_reset_tx;
		1100, // uint8_t  t_reset_rx;
		35,   // t_reset_delay
		30,   // uint8_t  t_write_low;
		100,  // uint8_t  t_write_slot;
		30,   // uint8_t  t_read_samp;
		100,  // uint8_t  t_read_slot;
};

DS_t DS;

void therm_init(void)
{
	uint8_t i;
	for (i = 0; i < 9; i++)
		DS.scratchpad[i] = 0;
	for (i = 0; i < 8; i++)
		DS.devID[i] = 0;
	DS.therm_pin = PINB0;
	PIN_HIGH(TRIG_PORT,TRIG_RESET_PIN);
	PIN_HIGH(TRIG_PORT,TRIG_READ_PIN);
	PIN_HIGH(TRIG_PORT,TRIG_BYTE_PIN);
	
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
	DS.therm_pin = newPin;
}
void therm_delay(uint16_t delay)
{
	while (delay--)
		asm volatile("nop");
}

uint8_t therm_reset()
{
	uint8_t i;
	PIN_LOW(TRIG_PORT,TRIG_RESET_PIN);
	THERM_OUTPUT_MODE(THERM_DDR, 1);
	THERM_LOW(THERM_PORT, DS.therm_pin);
	THERM_OUTPUT_MODE(THERM_DDR, DS.therm_pin);
	therm_delay(DS.t_reset_tx); //480 us
	THERM_INPUT_MODE(THERM_DDR, DS.therm_pin);
	//while (bit_is_clear(THERM_PIN, DS.therm_pin));
	therm_delay(DS.t_reset_delay);
	PIN_HIGH(TRIG_PORT,TRIG_RESET_PIN);
	//i = READ_PIN(THERM_PIN, DS.therm_pin);
	i = therm_read_n_times(10,5);
	PIN_LOW(TRIG_PORT,TRIG_RESET_PIN);
	//PIN_HIGH(THERM_PORT,1);
	therm_delay(DS.t_reset_rx); //480 us
	//Return the value read from the presence pulse (0=OK, 1=WRONG)
	PIN_HIGH(TRIG_PORT,TRIG_RESET_PIN);
	return i;
}

void therm_write_bit(uint8_t bit)
{
	//Pull line low for 1uS
	THERM_LOW(THERM_PORT, DS.therm_pin);
	THERM_OUTPUT_MODE(THERM_DDR, DS.therm_pin);
	therm_delay(DS.t_write_low);
	//If we want to write 1, release the line (if not will keep low)
	if (bit)
		THERM_INPUT_MODE(THERM_DDR, DS.therm_pin);
	//Wait for 60uS and release the line
	therm_delay(DS.t_write_slot);
	THERM_INPUT_MODE(THERM_DDR, DS.therm_pin);
}

uint8_t therm_read_n_times(uint8_t n, uint8_t threshold)
{
	uint8_t val;
	while(n--)
	{
		val += READ_PIN(THERM_PIN, DS.therm_pin);
		//val += (THERM_PIN & (1 << DS.therm_pin));
	}
	return (val >= threshold);
}

uint8_t therm_read_bit(void)
{	
	uint8_t bit = 0;
	
	PIN_LOW(TRIG_PORT,TRIG_READ_PIN);
	
	//Pull line low for 1uS
	THERM_LOW(THERM_PORT, DS.therm_pin);
	THERM_OUTPUT_MODE(THERM_DDR, DS.therm_pin);
	//Release line and wait for 14uS
	THERM_INPUT_MODE(THERM_DDR, DS.therm_pin);
	therm_delay(DS.t_read_samp);
	
	PIN_HIGH(TRIG_PORT,TRIG_READ_PIN);
	if (THERM_PIN & (1 << DS.therm_pin))
		bit = 1;
	PIN_LOW(TRIG_PORT,TRIG_READ_PIN);
	
	//bit = therm_read_n_times(1,0);	
	//Wait for 45uS to end and return read value
	therm_delay(DS.t_read_slot);
	PIN_HIGH(TRIG_PORT,TRIG_READ_PIN);
	
	return bit;
}

uint8_t therm_read_byte(void)
{
	uint8_t i = 8, n = 0;
	cli();
	PIN_LOW(TRIG_PORT,TRIG_BYTE_PIN);
	while (i--)
	{
		//Shift one position right and store read value
		n >>= 1;
		n |= (therm_read_bit() << 7);
	}
	PIN_HIGH(TRIG_PORT,TRIG_BYTE_PIN);
	sei();
	return n;
}

void therm_write_byte(uint8_t byte)
{
	uint8_t i = 8;
	cli();
	while (i--)
	{
		//Write actual bit and shift one position right to make the next bit ready
		therm_write_bit(byte & 1);
		byte >>= 1;
	}
	sei();
}

/////////////////////////////////////////////////////////////////////////
void therm_print_scratchpad()
{
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
{
	uint8_t i;
	rprintfStr("\"");
	for (i = 0; i < 8; i++)
	{
		rprintf("%d",DS.devID[i]);
		if (i==7)
			rprintf("\"");
		else
			rprintf(".");
	}
}

void therm_print_timing()
{
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
{
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

/////////////////////////////////////////////////////////////////////////
uint8_t therm_load_devID(uint8_t devNum){
	uint8_t no_error = 0, crc[1], i = 0;
	uint16_t address_sum = 0;
	for (i = 0; i < 8; i++)
	{
		//DS.devID[i] = eeprom_read_byte(&eeprom.dev[devNum][i]);
		DS.devID[i] = eeprom_read_byte(&eeprom.rom[DS.therm_pin][devNum][i]);
		address_sum += DS.devID[i];
	}
	if (address_sum > 0)
	{
		no_error = therm_crc_is_OK(DS.devID, crc, 7);		
	}
	//rprintf("therm_load_devID() no_error = %d\n",no_error);	
	return no_error;
}

void therm_save_devID(uint8_t devNum){
	uint8_t i;
	cli();
	for (i = 0; i < 8; i++){
		//eeprom_write_byte(&eeprom.dev[devNum][i], DS.devID[i]);
		eeprom_write_byte(&eeprom.rom[DS.therm_pin][devNum][i], DS.devID[i]);
	}
	sei();
}

void therm_set_devID(uint8_t *devID){
	uint8_t i;
	for (i = 0; i < 9; i++){
		DS.devID[i] = devID[i];		
	}
}

void therm_send_devID(){
	uint8_t i = 0;
	if (DS.devID[0] == 0)
	{
		therm_write_byte(THERM_CMD_SKIPROM);
	}
	else
	{
		therm_write_byte(THERM_CMD_MATCHROM);
		for (i = 0; i < 8; i++)
			therm_write_byte(DS.devID[i]);
	}
}

uint8_t therm_read_devID(){
	uint8_t no_error = 0, i = 0, crc[1];
	crc[0] = 0;

	therm_reset();
	//therm_send_devID();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_reset();
	therm_write_byte(THERM_CMD_READROM);
	for (i = 0; i < 8; i++)
		DS.devID[i] = therm_read_byte();

	no_error = therm_crc_is_OK(DS.devID, crc, 7);

	return no_error;
}

void therm_start_measurement(){
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
	if (therm_load_devID(devNum))
	{
		therm_reset();
		therm_start_measurement();
		_delay_ms(200);
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

uint8_t therm_read_result(int16_t *temperature){
	uint8_t no_error = 1;
	temperature[0] = 999;
	temperature[1] = 9999;

	therm_reset();

	if (no_error)
	{
		if(DS.devID[0] == DS18S20)
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
//						else if ((DS.scratchpad[0] == 2) & (DS.scratchpad[6] == 12))
//							temperature[0] += 1;
						else
							temperature[1] = (int16_t) (17500) - (int16_t) (DS.scratchpad[6] * 625);
					}
					else
						temperature[1] = (int16_t) (7500) - (int16_t) (DS.scratchpad[6] * 625);
				}
			}
			else if(DS.devID[0] == DS18B20)
				{
					no_error = therm_read_scratchpad(9);
					temperature[0] = (int16_t) (((DS.scratchpad[1] << 8) | (DS.scratchpad[0])) >> 4);
					temperature[1] = (int16_t) (((DS.scratchpad[1] << 8) | (DS.scratchpad[0])) & 15)*THERM_DECIMAL_STEPS_12BIT;
				}
			else if (DS.devID[0] == DS2438)
			{
				if (get_ds2438_temperature())
				{
					temperature[0] = (int16_t) (DS.scratchpad[2]);
					temperature[1] = (int16_t) (DS.scratchpad[1]);
				}
			}

			else
				{
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
		crc[0]  = therm_computeCRC8(scratchpad[i], crc[0]);
		id_sum += scratchpad[i];	
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

//the 1-wire search algo taken from:
//http://www.maxim-ic.com/appnotes.cfm/appnote_number/187


//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//

uint8_t OWFirst()
{
   // reset the search state
   LastDiscrepancy = 0;
   LastDeviceFlag = FALSE;
   LastFamilyDiscrepancy = 0;

   return OWSearch();
}


//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
uint8_t OWNext()
{
   // leave the search state alone
   return OWSearch();
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
	{
		ROM_NO[i] = 0;
	}
}
uint8_t OWSearch()
{
   unsigned char id_bit_number;
   unsigned char last_zero, rom_byte_number, search_result;
   unsigned char id_bit, cmp_id_bit;
   unsigned char rom_byte_mask, search_direction;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = 0;
   crc8 = 0;

   // if the last call was not the last one
   if (!LastDeviceFlag)
   {
	  // rprintf("\nLastDeviceFlag = 0\n");	
      // 1-Wire reset
      if (therm_reset()==0)
      {
		 rprintf("therm_reset()\n");	
         // reset the search
         LastDiscrepancy = 0;
         LastDeviceFlag = FALSE;
         LastFamilyDiscrepancy = 0;
         return FALSE;		 
	  }
	  else
		 //rprintf("sending search command\n");	

      // issue the search command
	  therm_write_byte(0xf0);

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
               if (id_bit_number < LastDiscrepancy)
                  search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
               else
                  // if equal to last pick 1, if not then pick 0
                  search_direction = (id_bit_number == LastDiscrepancy);

               // if 0 was picked then record its position in LastZero
               if (search_direction == 0)
               {
                  last_zero = id_bit_number;

                  // check for Last discrepancy in family
                  if (last_zero < 9)
                     LastFamilyDiscrepancy = last_zero;
               }
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1){
              ROM_NO[rom_byte_number] |= rom_byte_mask;
			  DS.devID[rom_byte_number] |= rom_byte_mask;
			}
            else
			{
              ROM_NO[rom_byte_number] &= ~rom_byte_mask;
			  DS.devID[rom_byte_number] &= ~rom_byte_mask;
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
                docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
                rom_byte_number++;
                rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7
	  // end of do-while loop 
	  /////////////////////////////////////////////////////////////////////////////////
	  
      // if the search was successful then
      if (!((id_bit_number < 65) || (crc8 != 0)))
      {
         // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
         LastDiscrepancy = last_zero;

         // check for last device
         if (LastDiscrepancy == 0)
            LastDeviceFlag = TRUE;

         search_result = TRUE;
      }
   }

   // if no device found then reset counters so next 'search' will be like a first
   if (!search_result || !ROM_NO[0])
   {
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      search_result = FALSE;
   }

   return search_result;
}

//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
uint8_t OWVerify()
{
   unsigned char rom_backup[8];
   unsigned char i,rslt,ld_backup,ldf_backup,lfd_backup;

   // keep a backup copy of the current state
   for (i = 0; i < 8; i++)
      rom_backup[i] = ROM_NO[i];
   ld_backup = LastDiscrepancy;
   ldf_backup = LastDeviceFlag;
   lfd_backup = LastFamilyDiscrepancy;

   // set search to find the same device
   LastDiscrepancy = 64;
   LastDeviceFlag = FALSE;

   if (OWSearch())
   {
      // check if same device found
      rslt = TRUE;
      for (i = 0; i < 8; i++)
      {
         if (rom_backup[i] != ROM_NO[i])
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
      ROM_NO[i] = rom_backup[i];
   LastDiscrepancy = ld_backup;
   LastDeviceFlag = ldf_backup;
   LastFamilyDiscrepancy = lfd_backup;

   // return the result of the verify
   return rslt;
}

void therm_test_func(void)
{
	uint8_t i;
	//rprintf("therm_test_func()\n");
		for (i = 0; i < 8; i++)
		{
			rprintf("%d",ROM_NO[i]);
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
   crc8 = dscrc_table[crc8 ^ value];
   return crc8;
}
