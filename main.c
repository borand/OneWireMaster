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

#define FW_VERSION "owire 15.12.12"

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
	Flags.print_json     = 0;
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
	DDRC  = 0xff;
	PORTC = 0xff;

	//cbi(DDRD,PB2);
	cbi(PORTB,PB5);
	//cbi(DDRD,PB2);
	//cbi(DDRD,PB3);

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
	cmdlineAddCommand("delay",  OneWireDelay);
	cmdlineAddCommand("rom",    OneWireReadRom);
	cmdlineAddCommand("reset",  OneWireReset);
	cmdlineAddCommand("load",   OneWireLoadRom);
	cmdlineAddCommand("sp",     OneWirerintScratchPad);
	cmdlineAddCommand("save",   SaveThermometerIdToRom);
	cmdlineAddCommand("start",  StartTemperatureMeasurement);
	cmdlineAddCommand("temp",   GetTemperature);
	cmdlineAddCommand("data",   GetOneWireMeasurements);
	cmdlineAddCommand("pin",    ChangeTmermPin);
	cmdlineAddCommand("rp",     OneWireReadPage);
	cmdlineAddCommand("wp",     OneWireWritePage);
	cmdlineAddCommand("search", OneSearch);
	cmdlineAddCommand("timing", OneWirePrintTimingTabel);
	cmdlineAddCommand("settiming", OneWireSetTimingTabel);
	
	//cli();
	therm_init();
	GetFW();
	cmdlinePrintPrompt();
	CmdLineLoop();
	return 0;
}
void PrintJson(void)
{
	uint8_t arg1 = (uint8_t) cmdlineGetArgInt(1);
	Flags.print_json = arg1;
}
void CmdLineLoop(void)
{
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
				OneWireReset();			
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

void GetFW(void){
	rprintfProgStrM("\"");
	rprintfProgStrM(FW_VERSION);
	rprintfProgStrM("\"");
	cmdlinePrintPromptEnd();
}
void GetIDN(void){
	PrintLabel(&eep_dev_sn[0]);
	cmdlinePrintPromptEnd();
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
void test(void)
{
	uint8_t arg1 = (uint8_t) cmdlineGetArgInt(1);
	uint8_t arg2 = (uint8_t) cmdlineGetArgInt(2);	
	therm_load_devID(arg1);
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
	stop = cmdlineGetArgHex(2);

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
//////////////////////////////////////////
void ResetCounters(void){
	TCNT1                = 0;
	timer1_ovf_count     = 0;
	rprintfProgStrM("1");
	cmdlinePrintPromptEnd();
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
void ChangeTmermPin(void)
{
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
	uint8_t i, device_count = 0, loop_count=0;
	int8_t devNum = 0;//(int8_t) cmdlineGetArgInt(1);
	
	if(Flags.print_json)
	{
		rprintfProgStrM("[");
	}
	else
	{
		rprintfCRLF();
	}
	
	for (i = 1; i <= MAX_NUMBER_OF_1WIRE_DEVICES; i++)
	{
		if (therm_load_devID(i) == 1)
		{
			loop_count++;
			if(Flags.print_json)
			{
				json_open_bracket();
				therm_print_devID();json_comma();
				therm_read_result(t);json_comma();
				therm_print_scratchpad();
				json_end_bracket();
				json_comma();
			}
			else{
				rprintf("%d : ", loop_count);
				therm_print_devID();
				rprintfProgStrM(" : ");
				therm_read_result(t);
				rprintfProgStrM(" : ");
				therm_print_scratchpad();
				rprintfCRLF();
			}
		}
	}
	if(Flags.print_json)
	{
		rprintfProgStrM("[\"0\",0,0]]");
		cmdlinePrintPromptEnd();
	}
	else
	{
		rprintfCRLF();
		cmdlinePrintPromptEnd();
	}
}
void GetOneWireMeasurements(void)
{
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
	uint8_t pin, i, done=0;

	for (pin = 0; pin < 3; pin++)
	{		
		therm_set_pin(pin);
		if (Flags.print_json) 
			rprintfProgStrM("[");		
		done = 0;
		for (i = 0; i < 20, done == 0; i++)
		{
			if (Flags.print_json)
			{
				rprintf("[%d,",i);
					if (therm_load_devID(i))
						therm_print_devID();
					else
						rprintfProgStrM("[]");

					if (i<MAX_NUMBER_OF_1WIRE_DEVICES-1)
						rprintf("],");
					else
						rprintfProgStrM("]");
			}
			else
				{	
					if (therm_load_devID(i)){
						rprintf("%d, %d, ",pin,i);
						therm_print_devID();
						rprintfCRLF();
					}else{
						done = 1;
					}
				}
				if (Flags.print_json) 
					rprintfProgStrM("]");				
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
	therm_save_devID(devNum);
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
void OneSearch(void){	
	uint8_t devNum=0, pin;
	therm_search_init();
	for (pin = 0; pin < 3; pin++)
	{
		therm_set_pin(pin);
		devNum=0;
		while(OWNext())
		{			
			therm_save_devID(devNum);		
			therm_test_func();
			devNum++;
		}
	}
	cmdlinePrintPromptEnd();
}
void OneWireReset(void)
{
	rprintf("\ntherm_reset = %d\n",therm_reset());
}

void OneWireDelay(void){
	uint16_t  pin = (uint16_t) cmdlineGetArgInt(1);
	uint16_t  val = (uint16_t) cmdlineGetArgInt(2);
	PIN_LOW(THERM_PORT,pin);
	therm_delay(val);
	PIN_HIGH(THERM_PORT,pin);
}

void OneWirePrintTimingTabel(void)
{
	therm_print_timing();
}

void OneWireSetTimingTabel(void){
	uint8_t  time      = (uint8_t)  cmdlineGetArgInt(1);
	uint16_t interval  = (uint16_t) cmdlineGetArgInt(2);
	therm_set_timing(time, interval);
}