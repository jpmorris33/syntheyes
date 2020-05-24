//
//  Synth Eyes for Raspberry Pi - GPIO version
//
//  This is a shim to compile the .ino file via GCC on the Raspberry Pi  
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wiringPi.h>
#include "arduino.h"

// Override the defaults
#define OVERRIDE_PINS
#define STARTLED_PIN 2
#define EYEROLL_PIN 3
#define ANNOYED_PIN 1

SPIlib SPI;

#include "../syntheyes2.ino"

void eyesSetup() {
for(int ctr=0;states[ctr].anim;ctr++) {
	if(states[ctr].pin) {
		pullUpDnControl(states[ctr].pin,PUD_UP);
	}
}
}
