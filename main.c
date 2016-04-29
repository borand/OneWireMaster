/*
 * main.c
 *
 *  Created on: Sep 11, 2009
 *      Author: Andrzej
 This firmware is based on C-Language Function Library for Atmel AVR Processors written by Pascal Stang
 http://www.procyonengineering.com/embedded/avr/avrlib/.

 Firmware target AVR mega328 16MHz.

 The application is used to interface AVR with onewire devices.  
 It provides basic command shell for searching and reading temperatures on the 1wire network.
 By default all output is returned in JSON format for easy parsing by higher level software.
 */
#include "global.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>
#include "uart.h"
#include "rprintf.h"
#include "vt100.h"
#include "cmdline.h"
#include "timer.h"
#include "onewire.h"
#include "main.h"

#define FW_VERSION "\"owire 15.12.12\""

Label_t __attribute__((section (".eeprom"))) eeprom_sn =
{
		"owire 1",  // t_conv ms
};

////////////////////////////////////////////////////////////////
// INTERRUPT CONTROL
void Timer0Func(void)
{
	if (timer0GetOverflowCount() >= timer0_ovf_count)
	{
		if (Flags.stream_timer_0)
		{
			timer0ClearOverflowCount();
			Flags.print_temp = 1;
			therm_reset();
			therm_start_measurement();
		}
	}
}

////////////////////////////////////////////////////////////////
// MAIN
//
int main(void)
{
	uartInit(); //Initialize UART
	uartSetBaudRate(115200);//Default Baudrate
	rprintfInit(uartSendByte);
	cmdlineInit();
	cmdlineSetOutputFunc(uartSendByte);
	vt100Init();
	vt100ClearScreen();

	///////////////////////////////////////////////////////
	// VARIABLE INIT
	Flags.print_temp     = 0;
	Flags.print_json     = 1;
	Flags.stream_timer_0 = 0;

	///////////////////////////////////////////////////////
	// TIMER0
	
	timer0_ovf_count = 1000;
	timer0Init();
	timerAttach(0,Timer0Func);
	timer0SetPrescaler(TIMER_CLK_DIV1024);

	///////////////////////////////////////////////////////
	DDRB  = _BV(PB0) | _BV(PB1) | _BV(PB5);
	PORTD = _BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7);
	
	// PORT C is used as output for trigger signals
	DDRC  = 0xff;
	PORTC = 0xff;

	cbi(PORTB,PB5);

	// GENERIC COMMANDS
	cmdlineAddCommand("help", HelpFunction);
	cmdlineAddCommand("idn",  GetIDN);
	cmdlineAddCommand("getfw",GetFW);
	cmdlineAddCommand("setsn",SetDevSNs);
	cmdlineAddCommand("getsn",GetDevSNs);

	cmdlineAddCommand("test", test);
	cmdlineAddCommand("json", PrintJson);
	cmdlineAddCommand("poke", Poke);
	cmdlineAddCommand("peek", Peek);
	cmdlineAddCommand("dump", Dump);
	cmdlineAddCommand("stream", StreamingControl);
	cmdlineAddCommand("interval", SetInterval);

	//////////////////////////////////////////////////////////////
	//
	cmdlineAddCommand("delay",   OneWireDelay);
	cmdlineAddCommand("wbit",    OneWireWriteBit);
	cmdlineAddCommand("rbit",    OneWireReadBit);
	cmdlineAddCommand("wbyte",   OneWireWriteByte);
	cmdlineAddCommand("rom",     OneWireReadRom);
	cmdlineAddCommand("reset",   OneWireReset);
	cmdlineAddCommand("load",    OneWireLoadRom);
	cmdlineAddCommand("clearrom",OneWireClearRom);
	cmdlineAddCommand("sp",      OneWirerintScratchPad);
	cmdlineAddCommand("save",    SaveThermometerIdToRom);
	cmdlineAddCommand("start",   StartTemperatureMeasurement);
	cmdlineAddCommand("temp",    GetTemperature);
	cmdlineAddCommand("data",    GetOneWireMeasurements);
	cmdlineAddCommand("pin",     ChangeTmermPin);
	cmdlineAddCommand("rp",      OneWireReadPage);
	cmdlineAddCommand("wp",      OneWireWritePage);
	cmdlineAddCommand("search",  OneSearch);
	cmdlineAddCommand("timing",  OneWirePrintTimingTabel);
	cmdlineAddCommand("settime", OneWireSetTimingTabel);
	
	//total number of devices found on the bus
	uint8_t i;
	for (uint8_t pin = 0; pin < MAX_NUMBER_OF_1WIRE_PORTS; pin++)
	{
		therm_set_pin(pin);
		i = 0;
		while(therm_load_devID(pin, i))
		{
			i++;
			devices_found++;
		}
	}
	//cli();
	therm_init();
	//GetFW();
	cmdlinePrintPrompt();
	CmdLineLoop();
	return 0;
}
void HelpFunction(void)
{
	rprintfProgStrM("Instant commands:\n");
	rprintfProgStrM("C                : clear screen\n");
	rprintfProgStrM("R                : send reset pulse\n");
	rprintfProgStrM("S                : perform binary search\n");
	rprintfProgStrM("Z                : reset command number to zero\n");

	rprintfProgStrM("\nCommands:\n");
	rprintfProgStrM("idn              : prints device ID and version info\n");
	rprintfProgStrM("test             : test function\n");
	rprintfProgStrM("peek [reg]       : returns dec, hex and bin value of register\n");
	rprintfProgStrM("poke [reg] [val] : sets register value to [val] \n");
	rprintfProgStrM("test             : test function\n");

	rprintfProgStrM("stream           : start streaming\n");

	rprintfProgStrM("\n\nOnewire Commands:\n");
	rprintfProgStrM("rom            : read rom of a single device\n");
	rprintfProgStrM("load           : load eeprom content\n");
	rprintfProgStrM("sp             : show scratch pad\n");
	rprintfProgStrM("save           : save scratchpad to eeprom\n");
	rprintfProgStrM("start          : start temperature measurement\n");
	rprintfProgStrM("temp           : read temperatures\n");
	rprintfProgStrM("data           : read data from 3824 device\n");
	rprintfProgStrM("rp             : read specific page from 3824 device\n");
	rprintfProgStrM("wp             : write data to specified page\n");
}
void CmdLineLoop(void){
	uint8_t  c;
	// main loop
	while (1)
	{
		// pass characters received on the uart (serial port)
		// into the cmdline processor
		
		if (Flags.print_temp)
		{
			Flags.print_temp = 0;
			rprintfProgStrM("owtemp\",\"data\":");
			delay_ms(1000);
			GetTemperature();			
			cmdlinePrintPrompt();
		}

		while (uartReceiveByte(&c))
		{
			switch (c)
			{
			{
			case 'C':
				vt100ClearScreen();
				vt100SetCursorPos(1, 1);
				cmdlinePrintPrompt();
				break;
			case 'R':
				OneWireReset();			
				break;
			case 'S':
				OneSearch();			
				break;
			case 'Z':
				cmdlineResetPrompt();
				cmdlinePrintPrompt();
				break;
			}
			default:
				cmdlineInputFunc(c);
			}
		}
		// run the cmdline execution functions
		cmdlineMainLoop();
	}
}

void GetFW(void){	
	rprintfProgStrM(FW_VERSION);	
	cmdlinePrintPromptEnd();
}
void GetIDN(void){
	//PrintLabel(&eep_dev_sn[0]);
	PrintLabel(&eeprom_sn);
	cmdlinePrintPromptEnd();
}
void PrintJson(void)
{
	uint8_t arg1 = (uint8_t) cmdlineGetArgInt(1);
	Flags.print_json = arg1;
}
////////////////////////////////////////////////////////////////
//Saving serial numbers
void SetDevSNs(void){

	uint8_t *port  = cmdlineGetArgStr(1);
	uint8_t devNum = (uint8_t) cmdlineGetArgInt(2);
	uint8_t *label = cmdlineGetArgStr(3);
	Label_t Label;
	if (port[0] == 'l') // assign location
		{
			strcpy(Label.label,label);
			eeprom_write_block(&Label,&eep_dev_location[0],sizeof(Label_t));
			rprintfProgStrM("\"OK\"");
		}
	else if (port[0] == 's') // asign serial number
		{
			strcpy(Label.label,label);
			eeprom_write_block(&Label,&eep_dev_sn[0],sizeof(Label_t));
			rprintfProgStrM("\"OK\"");
		}
	else
	{
		rprintfProgStrM("\"ERROR - Invalid syntax label [a|b|d|i|l|s] devNum label_text\"");
	}	
	cmdlinePrintPromptEnd();
}

void GetDevSNs(void){
	uint8_t  i;

	rprintfProgStrM("{\"SN\":");
    PrintLabel(&eep_dev_sn[0]);
	rprintfProgStrM(",\"location\":");
    PrintLabel(&eep_dev_location[0]);
	  
    json_end_bracket();
	json_end_bracket();
	rprintfProgStrM("}");
	cmdlinePrintPromptEnd();
}
////////////////////////////////////////////////////////////////
//JSON utility functions
void PrintLabel(Label_t *eep_label){
	Label_t Label;
	rprintfProgStrM("\"");
	eeprom_read_block(&Label,eep_label,sizeof(Label_t));
	rprintfStr(Label.label);
	rprintfProgStrM("\"");
}

////////////////////////////////////////////////////////////////
//Testing and utility functions
void test(void){
	//uint8_t arg1 = (uint8_t) cmdlineGetArgInt(1);
	//uint8_t arg2 = (uint8_t) cmdlineGetArgInt(2);
	//therm_load_devID(arg1, arg2);
	therm_test();
	//therm_load_devID(arg1, arg2);
	//therm_print_devID();
}
void Poke(void) {

	uint16_t address = 0;
	uint8_t value = 0;

	address = cmdlineGetArgHex(1);
	value = cmdlineGetArgHex(2);
	_SFR_MEM8(address) = value;
}
void Peek(void) {

	uint16_t address = 0;
	uint8_t value = 0;

	address = cmdlineGetArgHex(1);
	value = _SFR_MEM8(address);
	rprintf("[%d,\"0x%x\"]", value, value);
	cmdlinePrintPromptEnd();
}
void Dump(void) {

	uint16_t address, start, stop;
	uint8_t value, add_h, add_l;
	uint8_t col = 0;

	start = cmdlineGetArgHex(1);
	stop  = cmdlineGetArgHex(2);

	for (address = start; address <= stop; address++) {
		value = _SFR_MEM8(address);
		if (col == 0) {
			add_h = (address >> 8);
			add_l = (address & 0xff);
			rprintf("0x%x%x 0x%x", add_h, add_l, value);
		} else {
			rprintf(" 0x%x", value);
		}
		col++;
		if (col == 8) {
			rprintf("\r\n");
			col = 0;
		}
	}
	rprintf("\r\n");
}

/////////////////////////////////////////////////////////////////////////////////////
// STREAMING FUNCTION
void StreamingControl(void){
	Flags.stream_timer_0 = (uint8_t) cmdlineGetArgInt(1);
	rprintf("%d",Flags.stream_timer_0);
	cmdlinePrintPromptEnd();
}
void SetInterval(void){
	timer0_ovf_count = (uint16_t) cmdlineGetArgInt(1);
	if (timer0_ovf_count == 0)
	{
		eeprom_read_dword(&timer0_ovf_count);
		rprintf("%d",timer0_ovf_count);
	}
	else
	{
		eeprom_write_dword(&eep_timer0_ovf_count, timer0_ovf_count);
		rprintf("%d",timer0_ovf_count);
		cmdlinePrintPromptEnd();
	}
}
////////////////////////////////////////////////////////////////
// ONE WIRE DEVICES
//
void ChangeTmermPin(void){

	therm_set_pin((uint8_t)cmdlineGetArgInt(1));
}
void StartTemperatureMeasurement(void){
	therm_reset();
	therm_start_measurement();
	rprintfProgStrM("1");
	cmdlinePrintPromptEnd();
}
void GetTemperature(void){
	int16_t t[2];
	int8_t devNum = 0;//(int8_t) cmdlineGetArgInt(1);
	uint8_t pin, i;
	
	if(Flags.print_json)	
		rprintfProgStrM("[");
	else
		rprintfCRLF();

	for (pin = 0; pin < MAX_NUMBER_OF_1WIRE_PORTS; pin++)
	{	
		therm_set_pin(pin);
		i = 0;
		while(therm_load_devID(pin, i))
		{
			if (Flags.print_json)
			{
				rprintfProgStrM("[");
				therm_print_devID();
				json_comma();
				therm_read_result(t);
				json_comma();
				rprintf("%d,%d,",pin,i);
				therm_print_scratchpad();
				rprintfProgStrM("]");
			}
			else
			{					
				rprintf("%d, %d, ",pin,i);				
				therm_print_devID();
				rprintfProgStrM(" : ");
				therm_read_result(t);
				rprintfProgStrM(" : ");
				therm_print_scratchpad();
				rprintfCRLF();
			}
			i++;
			devNum++;
			if ((Flags.print_json) && (devNum < devices_found))
			{
				json_comma();
			}

		}
	}
	if (Flags.print_json)
	{
		rprintfProgStrM("]");
	}
	cmdlinePrintPromptEnd();
}
void GetOneWireMeasurements(void){
	test_ds2438();
	cmdlinePrintPromptEnd();
}
void OneWireReadRom(void){
	if(therm_read_devID())
		therm_print_devID();
	else
	{
		therm_print_devID();
		rprintf("CRC Error");
	}
	cmdlinePrintPromptEnd();
}
void OneWireLoadRom(void){
	
	uint8_t pin, i;
	for (pin = 0; pin < MAX_NUMBER_OF_1WIRE_PORTS; pin++)
	{	
		if (Flags.print_json) 
			rprintfProgStrM("[");		
		i = 0;
		while(therm_load_devID(pin, i))
		{
			if (Flags.print_json)
			{
				rprintf("[%d,%d,",pin,i);
				therm_print_devID();
				rprintfProgStrM("]");
			}
			else
			{					
				rprintf("%d, %d, ",pin,i);
				therm_print_devID();
				rprintfCRLF();
			}
			if (Flags.print_json) 
				rprintfProgStrM("]");
			i++;				
		}
	}
	cmdlinePrintPromptEnd();
}
void SaveThermometerIdToRom(void){
	uint8_t  devNum = (uint8_t) cmdlineGetArgInt(1);
	uint8_t  devID[8], i;
	for(i=0;i<8;i++)
		devID[i] = (uint8_t) cmdlineGetArgInt(i+2);

	if (devID[0] != 0)
		therm_set_devID(devID);
	therm_save_devID(0,devNum);
	OneWireLoadRom();
}
void OneWireReadPage(void){
	uint8_t  page = (uint8_t) cmdlineGetArgInt(1);

	recal_memory_page(page);
	therm_print_scratchpad();
	cmdlinePrintPromptEnd();
}
void OneWireWritePage(void){
	uint8_t  page = (uint8_t) cmdlineGetArgInt(1);
	uint8_t  val  = (uint8_t) cmdlineGetArgInt(2);
	
    write_to_page(page, val);
	_delay_ms(1);
	recal_memory_page(page);
	therm_print_scratchpad();
	cmdlinePrintPromptEnd();
}
void OneWirerintScratchPad(void){
	therm_print_scratchpad();
	cmdlinePrintPromptEnd();
}
void OneWireClearRom(void){
	uint8_t devNum=0, pin;
	
	// Blank the entire eprom prior to search
	therm_search_init();
	for (pin = 0; pin < MAX_NUMBER_OF_1WIRE_PORTS; pin++)
	{
		for (devNum = 0; devNum < MAX_NUMBER_OF_1WIRE_DEVICED_PER_PORT; devNum++)
		therm_save_devID(pin, devNum);
	}
}
void OneSearch(void){	
	uint8_t devNum=0, pin;
	therm_search_init();
	devices_found = 0;

	if(Flags.print_json)
		rprintfProgStrM("[");
	else
		rprintfCRLF();

	for (pin = 0; pin < MAX_NUMBER_OF_1WIRE_PORTS; pin++)
	{
		therm_set_pin(pin);
		devNum=0;
		while(therm_find_next_dev())
		{

			therm_save_devID(pin, devNum);
			if (Flags.print_json)
			{
				rprintfProgStrM("[");
				rprintf("%d,%d,",pin,devNum);
				therm_print_devID();
				rprintfProgStrM("]");
			}
			else
			{
				rprintf("%d,%d,",pin,devNum);
				therm_print_devID();
				rprintfCRLF();
			}

			devNum++;
			devices_found++;
			if (Flags.print_json)
				json_comma();

		}
	}
	if (Flags.print_json)
	{
		rprintfProgStrM("[-1,-1]]");
	}
	cmdlinePrintPromptEnd();
}
void OneWirePrintTimingTabel(void){

	therm_print_timing();
}
void OneWireSetTimingTabel(void){
	uint8_t  time      = (uint8_t)  cmdlineGetArgInt(1);
	uint16_t interval  = (uint16_t) cmdlineGetArgInt(2);
	therm_set_timing(time, interval);
}

////////////////////////////////////////////////////////////////////////////
// The following functions are useful for calibrating the 1wire timing
// with the aid of oscilloscope or logic analyzer.
void OneWireDelay(void){
	/*
	val    measured
	1      1.28
	10     4.64
	20     8.40
	50     19.7
	100    38.4
	200    76.4
	500    189
	1000   376

	delay_us(count) = 0.3752*count + 0.9983;
	
	Calculate count for desired delay 
	count(desired_delay_us) = 2.665*desired_delay_us - 2.66;

	*/	
	uint16_t  pin = (uint16_t) cmdlineGetArgInt(1);
	uint16_t  val = (uint16_t) cmdlineGetArgInt(2);
	
	therm_set_pin(pin);
	TRIG_LOW(TRIG_RESET_PIN);
	therm_delay(val);
	TRIG_HIGH(TRIG_RESET_PIN);
}
void OneWireReset(void){
	// issues a reset command
	rprintf("therm_reset() = %d\n",therm_reset());
}
void OneWireWriteBit(void){	
	uint8_t  bit = (uint8_t)cmdlineGetArgInt(1);
	OneWireReset();
	therm_write_bit(bit);
}
void OneWireReadBit(void){
	uint8_t  bit = (uint8_t)cmdlineGetArgInt(1);
	rprintf("OneWireReadBit = %d\n",bit);
	OneWireReset();
	therm_read_bit();
}
void OneWireWriteByte(void){
	uint8_t  byte = (uint8_t)cmdlineGetArgInt(1);
	rprintf("OneWireWriteByte = %d\n",byte);
	OneWireReset();
	therm_write_byte(byte);
}
