/*
 * main.h
 *
 *  Created on: Apr 13, 2014
 *      Author: andrzej
 */

#ifndef MAIN_H_
#define MAIN_H_
#define MAX_NUMBER_OF_1WIRE_DEVICES 20

typedef struct {
	uint8_t label[20];
} Label_t;

uint8_t EEMEM eep_devid = 1;
uint16_t EEMEM eep_timer0_ovf_count = 200;
Label_t eep_dev_sn[1] EEMEM;
Label_t eep_dev_location[1] EEMEM;
Label_t eep_adc_sn[NUM_OF_ADCS] EEMEM;

uint8_t  timer1_ovf_count;
uint16_t timer0_ovf_count;
uint16_t timer1_count;
uint16_t devices_found;


void SetDevSNs(void);
void GetDevSNs(void);
void GetFW(void);

void CmdLineLoop(void);
void HelpFunction(void);
void GetIDN(void);
void StreamingControl(void);

void test(void);

void Poke(void);
void Peek(void);
void Dump(void);

void OneWireDelay(void);
void StartTemperatureMeasurement(void);
void GetTemperature(void);
void GetOneWireMeasurements(void);
void OneWireLoadRom(void);
void SaveThermometerIdToRom(void);
void OneWireReadRom(void);
void OneWireReadPage(void);
void OneWireClearRom(void);
void OneWirerintScratchPad(void);
void OneWireWritePage(void);
void OneSearch(void);
void OneWireReset(void);
void ChangeTmermPin(void);
void OneWirePrintTimingTabel(void);
void OneWireSetTimingTabel(void);
void PrintLabel(Label_t *eep_label);
void PrintJson(void);

void OneWireWriteBit(void);
void OneWireWriteByte(void);
void OneWireReadBit(void);

void SetInterval(void);

#endif /* MAIN_H_ */
