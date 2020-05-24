//
//  Synth Eyes for Raspberry Pi - signalling version
//
//  This is a shim to compile the .ino file via GCC on the Raspberry Pi  
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <wiringPi.h>
#include "arduino.h"

// Use our own expression handler
#define CUSTOM_EXPRESSION_HANDLER

SPIlib SPI;

static char expression[256];
void eye_handler (int signum);

#include "../syntheyes2.ino"

void eyesSetup() {
// Set up signal handler

struct sigaction eyeAction;

eyeAction.sa_handler = eye_handler;
sigemptyset (&eyeAction.sa_mask);
eyeAction.sa_flags = 0;

// This is horribly crude but it does work
// You can use the python plugin in the 'sopare' directory to make
// SOPARE invoke these.  However the reaction time is too slow for 
// the intended purpose.

sigaction(SIGUSR1, &eyeAction, NULL);
sigaction(SIGUSR2, &eyeAction, NULL);
sigaction(SIGURG, &eyeAction, NULL);
}

//
//  Set an expression when we get a signal
//

void eye_handler (int signum)
{
switch(signum) {
	case SIGUSR1:
		expression[ANNOYED_PIN] = 1;
		break;
	case SIGUSR2:
		expression[EYEROLL_PIN] = 1;
		break;
	case SIGURG:
		expression[STARTLED_PIN] = 1;
		break;
	default:
		break;
}
}


//
//  Maintain the expression changes independently
//

bool checkExpression(int pin) {
if(expression[pin]) {
	expression[pin]=0;
	return 1;
}
return 0;
}

