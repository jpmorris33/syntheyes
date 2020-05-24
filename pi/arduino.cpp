#include <stdio.h>
#include <stdlib.h>
#include "arduino.h"
#include <wiringPi.h>
#include <wiringPiSPI.h>

SPIlib::SPIlib() {
}

void SPIlib::begin() {
wiringPiSetup();
}

void SPIlib::beginTransaction(long speed) {
wiringPiSPISetup(0,speed/2); // half speed is more stable on BPI
}

void SPIlib::transfer(unsigned char *ptr, int len) {
wiringPiSPIDataRW(0,ptr,len);
}


void randomSeed(unsigned int r) {
	srand(r);
}

int random(int lowest, int highest) {
highest-=lowest;
if(highest < 1) {
	highest=1;
}
return (random() % highest) + lowest;
}
