//
//  Protogen Eyes for Arduino - simplified version 
//
//  Based on example code from  https://gist.github.com/nrdobie/8193350  among other sources
//
//  Copyright (c) 2023 J. P. Morris
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
//  CS - pin 9 (this can be changed, see below)
//  DIN - pin 11 (this is fixed by the SPI library) - send this output to both eyes
//  CLK - pin 13 (this is fixed by the SPI library) - send this output to both eyes
//
//  My panels were arranged with the drivers cascaded eye1->eye2->mouth1->mouth2->mouth3->mouth4->nostril ...and in reverse for the second face
//  however the image is drawn upside-down.
//  If your panels are in a different order, adjust the constants below (see 'panel order')
//


#include <SPI.h>

// Configurables, adjust to taste

#define FLIP_L_VERTICAL // Comment this out to have the left side displayed with no vertical mirroring
#define FLIP_R_VERTICAL // Comment this out to have the right side displayed with no vertical mirroring
//#define FLIP_L_HORIZONTAL
#define FLIP_R_HORIZONTAL

#define BRIGHTNESS  2     // Default brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)

#define CS_PIN 9       // Chip select pin

//
//  Panel positions for each part of the sprite
//
//  If the sprite appears garbled, with the wrong displays showing each corner
//  because your panels are wired differently to mine, you may need to adjust
//  the ID of each panel here, e.g. 0-3 instead of 3-0
//

#define NUM_PANELS 16 // This can be reduced if needed - writeDate should protect against overruns

// Panel order (as used in Quirk) - note that the highest number must be 15 (i.e. NUM_PANELS-1)
#define LEFT_EYE_1    1
#define LEFT_EYE_2    0
#define LEFT_MOUTH_1  5
#define LEFT_MOUTH_2  4
#define LEFT_MOUTH_3  3
#define LEFT_MOUTH_4  2
#define LEFT_NOSTRIL  6
#define RIGHT_NOSTRIL  7
#define RIGHT_MOUTH_4  11
#define RIGHT_MOUTH_3  10
#define RIGHT_MOUTH_2  9
#define RIGHT_MOUTH_1  8
#define RIGHT_EYE_2    13
#define RIGHT_EYE_1    12
#define RIGHT_CHEEK    14
#define LEFT_CHEEK     15

// System constants, you probably don't want to touch these

#define CMD_TEST      0x0f
#define CMD_INTENSITY 0x0a
#define CMD_SCANLIMIT 0x0b
#define CMD_DECODE    0x09
#define CMD_SHUTDOWN  0x0C

// Functions

void initPanels(int brightness);
void drawEyes();
void writeData(int addr, byte opcode, byte data);
void blit();

// transfer buffer for programming the display chip
unsigned char spidata[NUM_PANELS * 2];    // Dedicated to sprite data
unsigned char spicmd[NUM_PANELS * 2]; // Dedicated to commands

//
//  Sprite data
//
// These are monochrome sprites, 0 is dark, 1 is lit.
// Left side is the left side of the image.
// There is code to flip these so that the display is mirrored on the other side of the face.
// 
// Note that these are currently stored in dynamic memory so there's a 2KB limit on the Arduino Nano

// Right eye (facing left)
unsigned char eye[16] = {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000001,B11110000,
      B00000111,B11111100,
      B00000111,B00000100,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
};

// Nostril

unsigned char nostril[8] = {
      B00000000,
      B01111000,
      B01100000,
      B00000000,
      B00000000,
      B00000000,
      B00000000,
      B00000000,
};

unsigned char mouth[32] = {
      B00000000,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000100,
      B00000000,B00000000,B00000000,B00001000,
      B00000000,B00000000,B00000000,B00110000,
      B01000000,B00000000,B00000011,B11000000,
      B00100110,B00011100,B11001100,B00000000,
      B00011001,B11100011,B00110000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
};

unsigned char cheek[8] = {
      B00000000,
      B00111100,
      B01000010,
      B10111101,
      B01011010,
      B00100100,
      B00011000,
      B00000000,
};



//
//  Here's the actual implementation
//

void setup() {
  pinMode(CS_PIN,OUTPUT);
  
  // Set up data transfers
  SPI.begin();
  SPI.beginTransaction( SPISettings(16000000, MSBFIRST, SPI_MODE3 ) );

  initPanels(BRIGHTNESS);
}

void loop() {

   drawEyes();
   delay(1000);
}

//
//  Bit reversal table from https://forum.arduino.cc/index.php?topic=117966.0
//  For flipping the sprites.  This will take 7 cycles vs 2 for each a RAM read, should be fine
//

const byte reverse[256] PROGMEM = {
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

// Macros to help flip the image if desired

#ifdef FLIP_L_HORIZONTAL
  #define GET_LEFT(x) pgm_read_byte(&(reverse[x]))
#else
  #define GET_LEFT(x) x
#endif

#ifdef FLIP_R_HORIZONTAL
  #define GET_RIGHT(x) pgm_read_byte(&(reverse[x]))
#else
  #define GET_RIGHT(x) x
#endif

//
//  Draw both eyes
//

void drawEyes() {
  unsigned char *ptr_e = &eye[0];
  unsigned char *ptr_m = &mouth[0];
  unsigned char *ptr_n = &nostril[0];
  unsigned char *ptr_c = &cheek[0];

  unsigned char *old_e,*old_m,*old_n,*old_c;
  int pos;

  // With Quirk's current wiring the panels are both upside-down.
  // If you need to change this, edit how 'pos' is set below:

  for( int row=0;row<8; row++) {

#ifdef FLIP_L_VERTICAL
    pos = 8-row;
#else
    pos = row+1;
#endif

    // Record start of sprite positions for the other side of the face
    old_e=ptr_e;
    old_m=ptr_m;
    old_n=ptr_n;
    old_c=ptr_c;  

    // Send a row of data to the chip.  First parameter will be the panel number (0 to NUM_PANELS-1)
    // Second parameter will be the row of the display (1-8), third panel will be 8 pixels packed as a single byte
    
    writeData(LEFT_EYE_1,pos,GET_LEFT(*ptr_e++));
    writeData(LEFT_EYE_2,pos,GET_LEFT(*ptr_e++));
    writeData(LEFT_MOUTH_1,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_MOUTH_2,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_MOUTH_3,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_MOUTH_4,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_NOSTRIL,pos,GET_LEFT(*ptr_n++));
    writeData(LEFT_CHEEK,pos,GET_LEFT(*ptr_c++));

#ifdef FLIP_R_VERTICAL
    pos = 8-row;
#else
    pos = row+1;
#endif

    // Rewind to the start of each sprite to draw the other side
    ptr_e=old_e;
    ptr_m=old_m;
    ptr_n=old_n;
    ptr_c=old_c;

    writeData(RIGHT_EYE_1,pos,GET_RIGHT(*ptr_e++));
    writeData(RIGHT_EYE_2,pos,GET_RIGHT(*ptr_e++));
    writeData(RIGHT_MOUTH_1,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_MOUTH_2,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_MOUTH_3,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_MOUTH_4,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_NOSTRIL,pos,GET_RIGHT(*ptr_n++));
    writeData(RIGHT_CHEEK,pos,GET_LEFT(*ptr_c++));  // Treat as left, don't mirror it (use GET_RIGHT if you don't want that)
    
    blit(); // Write it to the display
  }
 
}




//
//  Set up the display panels
//

void initPanels(int brightness) {
  // Initialise the panels

  setRegister(CMD_TEST,0);
  setRegister(CMD_DECODE,0);  // Disable BCD decoder so we can sent sprite data instead
  setRegister(CMD_INTENSITY,brightness);
  setRegister(CMD_SCANLIMIT,7);
  setRegister(CMD_SHUTDOWN,1);   // 0 turns it off, 1 turns it on
}


//
//  Send a command or data to the display chip
//

void writeData(int addr, byte opcode, byte data) {

  if(addr >= NUM_PANELS) {
     return;
  }

  addr = (NUM_PANELS-1) - addr;
  int offset=addr*2;
  
  spidata[offset]=opcode;
  spidata[offset+1]=data;
}

void blit() {
 
  // Blit it to the display
  digitalWrite(CS_PIN,LOW);
  SPI.transfer(spidata,NUM_PANELS*2);
  digitalWrite(CS_PIN,LOW);
  digitalWrite(CS_PIN,HIGH);
}

//
//  Send a command or data to all the display chips
//

void setRegister(byte opcode, byte data) {
  digitalWrite(CS_PIN,LOW);
  for(int ctr=0;ctr<NUM_PANELS;ctr++) {
    spicmd[ctr*2]=opcode;
    spicmd[(ctr*2)+1]=data;
  }
  SPI.transfer(spicmd,NUM_PANELS*2);
  digitalWrite(CS_PIN,LOW);
  digitalWrite(CS_PIN,HIGH);
}
