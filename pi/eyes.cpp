//
//  Synth Eyes for Arduino
//  V2.3 - With sprite-flipping, reactions, lazy updates and new state system
//
//  Based on example code from  https://gist.github.com/nrdobie/8193350  among other sources
//
//  Copyright (c) 2020 J. P. Morris
// 
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

//
//  Wire the driver board to the arduino with the following pinout:
//
//  CS - pin 10 (this can be changed, see below)
//  DIN - pin 11 (this is fixed by the SPI library)
//  CLK - pin 13 (this is fixed by the SPI library)
//
//  My panels were arranged with the drivers cascaded BR->BL->TR->TL,
//  however the image is drawn upside-down.
//  If your panels are in a different order, adjust the constants below
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
