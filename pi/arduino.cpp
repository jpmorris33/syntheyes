#include <stdio.h>
#include <stdlib.h>
#include "arduino.h"

SPIlib::SPIlib() {
}

void SPIlib::begin() {
printf("NOT YET\n");
}

void SPIlib::beginTransaction(int nothing) {
printf("NOT YET\n");
}

void SPIlib::transfer(unsigned char *ptr, int len) {
printf("NOT YET\n");
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
