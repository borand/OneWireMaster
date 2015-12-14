/*
 * global.h
 *
 *  Created on: Sep 11, 2009
 *      Author: Andrzej
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_
#include <avr/io.h>
// global AVRLIB defines
#include "avrlibdefs.h"
// global AVRLIB types definitions
#include "avrlibtypes.h"

#define F_CPU 16000000UL 

// CYCLES_PER_US is used by some short delay loops
#define CYCLES_PER_US ((F_CPU+500000)/1000000) 	// cpu cycles per microsecond

// size of command database
// (maximum number of commands the cmdline system can handle)
#define CMDLINE_MAX_COMMANDS	30

// maximum length (number of characters) of each command string
// (quantity must include one additional byte for a null terminator)
#define CMDLINE_MAX_CMD_LENGTH	8

// allotted buffer size for command entry
// (must be enough chars for typed commands and the arguments that follow)
#define CMDLINE_BUFFERSIZE		80

// number of lines of command history to keep
// (each history buffer is CMDLINE_BUFFERSIZE in size)
// ***** ONLY ONE LINE OF COMMAND HISTORY IS CURRENTLY SUPPORTED
#define CMDLINE_HISTORYSIZE		2

#define DEBUG 0
#define NUM_OF_ADCS 5

typedef struct {
	uint8_t  print_temp;
	uint8_t  print_json;
	uint8_t  stream_timer_0;
} Flags_t;

Flags_t Flags;

#endif /* GLOBAL_H_ */
