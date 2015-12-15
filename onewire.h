#include <avr/io.h>
#include <stdio.h>
#include "rprintf.h"

#ifndef F_CPU
#define F_CPU 16000000UL 		//Your clock speed in Hz (3Mhz here)
#endif

#define LOOP_CYCLES 16		//Number of cycles that the loop takes
#define us(num) (num/(LOOP_CYCLES*(1/(F_CPU/1000000.0))))

/* list of these commands translated into C defines:*/
#define THERM_CMD_CONVERTTEMP 0x44
#define THERM_CMD_CONVERT_VOLTAGE 0xb4
#define CMD_RECALL_PAGE 0xb8
#define THERM_CMD_RSCRATCHPAD 0xbe
#define THERM_CMD_WSCRATCHPAD 0x4e
#define THERM_CMD_CPYSCRATCHPAD 0x48
#define THERM_CMD_RECEEPROM 0xb8
#define THERM_CMD_RPWRSUPPLY 0xb4
#define THERM_CMD_SEARCHROM 0xf0
#define THERM_CMD_READROM 0x33
#define THERM_CMD_MATCHROM 0x55
#define THERM_CMD_SKIPROM 0xcc
#define THERM_CMD_ALARMSEARCH 0xec
/* constants */
#define THERM_DECIMAL_STEPS_12BIT 625 //.0625

//**************************************************************************
// GENERIC MACROS
#define TRUE 1 //if !=0
#define FALSE 0

#define PIN_LOW(reg,  bit)  reg &=~(1<<bit)
#define PIN_HIGH(reg, bit)  reg |= (1<<bit)
#define READ_PIN(reg, bit)  reg &= (1<<bit)
#define TOGGLE(reg,bit)     reg ^= (_BV(bit))
#define THERM_INPUT_MODE(reg, bit)  reg &=~ (1<<bit)
#define THERM_OUTPUT_MODE(reg, bit) reg |=  (1<<bit)
#define THERM_LOW(reg, bit)  reg &=~(1<<bit)
#define THERM_HIGH(reg, bit) reg |=(1<<bit)

//#define THERM_PORT PORTB
//#define THERM_DDR  DDRB
#define THERM_PIN  PINB

//**************************************************************************
// Macros for generating trigger pulses for oscilloscope or logic analyzer.
//
#define TRIG_PORT      PORTC
#define TRIG_DDR       DDRC
#define TRIG_RESET_PIN PINC0
#define TRIG_READ_PIN  PINC1
#define TRIG_BYTE_PIN  PINC2 
#define TRIG_CMD_PIN   PINC3
 
#define TRIG_LOW(bit)  TRIG_PORT &=~(1<<bit)
#define TRIG_HIGH(bit) TRIG_PORT |= (1<<bit)

//**************************************************************************
// Common family codes for onewire devices
#define DS2438     38
#define DS18B20    40
#define DS18S20    16


//#define TRIG_LOW(bit)  TRIG_PORT&=~(1<<TRIG_PIN)
//#define TRIG_HIGH(bit) TRIG_PORT|=(1<<TRIG_PIN)
//#define THERM_DEBUG 1

typedef struct
{
	uint16_t t_conv;
	uint16_t t_reset_tx;
	uint16_t t_reset_rx;
	uint16_t t_reset_delay;
	uint8_t  t_write_low;
	uint8_t  t_write_slot;
	uint8_t  t_read_samp;
	uint8_t  t_read_slot;	
	uint8_t  rom[10][20][8];
} EE_RAM_t;

typedef struct
{
	uint8_t  scratchpad[9];
	uint8_t  dev_id[8];	
	int8_t   temp_digit;
	int16_t  temp_decimal;
	uint8_t  therm_port;
	uint8_t  therm_ddr;
	uint8_t  therm_pin;
	uint8_t  therm_pin_reg;
	
	uint16_t t_conv;
	uint16_t t_reset_tx;
	uint16_t t_reset_rx;
	uint16_t t_reset_delay;
	uint8_t  t_write_low;
	uint8_t  t_write_slot;
	uint8_t  t_read_samp;
	uint8_t  t_read_slot;
		
	uint8_t last_discrepancy;
	uint8_t last_family_discrepancy;
	uint8_t last_device_flag;
	uint8_t crc8;	
} DS_t;

void    therm_delay(uint16_t delay);
uint8_t therm_reset();
void    therm_init(void);
void    therm_search_init(void);
void    therm_write_bit(uint8_t bit);
uint8_t therm_read_bit(void);
uint8_t therm_read_byte(void);
void    therm_write_byte(uint8_t byte);
void    therm_print_scratchpad();
void    therm_print_devID();
void    therm_print_timing();
void    therm_set_timing(uint8_t time, uint16_t interval);
void    therm_set_pin(uint8_t newPin);
void    therm_test_func(void);
//
uint8_t therm_read_n_times(uint8_t n, uint8_t threshold);
uint8_t therm_read_devID();
void    therm_send_devID();
uint8_t therm_load_devID(uint8_t therm_pin, uint8_t devNum);
void    therm_set_devID(uint8_t *devID);
void    therm_save_devID(uint8_t devNum);
uint8_t therm_read_scratchpad(uint8_t numOfbytes);
void    therm_start_measurement();
uint8_t therm_read_result(int16_t *temperature);
uint8_t therm_read_temperature(uint8_t devNum, int16_t *temperature);

uint8_t therm_crc_is_OK(uint8_t *scratchpad, uint8_t *crc, uint8_t numOfBytes);
uint8_t therm_computeCRC8(uint8_t inData, uint8_t seed);


////////////////////////////////////////////////////////////////
// Search algorithm
uint8_t therm_find_first_dev(void);
uint8_t therm_find_next_dev(void);
uint8_t therm_run_tree_search(void);
uint8_t therm_verify_tree_search(void);
uint8_t docrc8(unsigned char value);

//////////////////////////////////////////////////////////////
// DS2438
//
void recal_memory_page(uint8_t page);
void test_ds2438(void);
void write_to_page(uint8_t page, uint8_t val);
uint8_t get_ds2438_temperature(void);

