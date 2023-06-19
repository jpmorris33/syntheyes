//
//  Protogen Eyes for Arduino
//  V3.0.0 - Use single CS line, add voice detector
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
//  If your panels are in a different order, adjust the constants below
//


#include <SPI.h>

#define ENABLE_PSYCHO    // Warning, mad protogen
//#define ENABLE_DEFENDER  // Shall we play a game?
//#define DEAD_PROTO     // This could be triggered by a GPIO pin but I made it for a photoshoot

// Configurables, adjust to taste

#define FLIP_L_VERTICAL // Comment this out to have the left side displayed with no vertical mirroring
#define FLIP_R_VERTICAL // Comment this out to have the right side displayed with no vertical mirroring
// #define FLIP_L_HORIZONTAL
#define FLIP_R_HORIZONTAL

#define BRIGHTNESS  2     // Default brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)
#define VOICE_BRIGHT 12   // Brightness to use when voice detected

#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds (20 default)
#define WAIT_IN_MS  60  // Delay per tick in milliseconds when waiting to blink again (60 default)
#define MIN_DELAY    5   // Minimum delay between blinks
#define MAX_DELAY    250 // Maximum delay between blinks

#define CS_PIN 9       // Chip select pin

#define VOICE_DETECTOR
#define VOICE_PIN A1
#define ADC_THRESHOLD 128 // Voice activation threshold


#ifndef OVERRIDE_PINS
	#define ANNOYED_PIN  8
  #define STARTLED_PIN 7
  #define KILL_PIN     6
  #define REVIVE_PIN   5
#endif

//
//  Panel positions for each part of the sprite
//
//  If the sprite appears garbled, with the wrong displays showing each corner
//  because your panels are wired differently to mine, you may need to adjust
//  the ID of each panel here, e.g. 0-3 instead of 3-0
//

#define NUM_PANELS 14

// Panel order (as used in Quirk)
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

// System constants, you probably don't want to touch these

#define CMD_TEST      0x0f
#define CMD_INTENSITY 0x0a
#define CMD_SCANLIMIT 0x0b
#define CMD_DECODE    0x09
#define CMD_SHUTDOWN  0x0C

// Functions

void initPanels(int brightness);
void drawEyes();
void getSpriteL(unsigned char *ptr, int blinkpos);
void getSpriteR(unsigned char *ptr, int blinkpos);
void writeData(int addr, byte opcode, byte data);
void blit();
//void sendData(int cs, int addr, byte opcode, byte data);
void wait(int ms, bool interruptable);
void getNextAnim();
bool checkExpression(int pin);

// System state variables

#define WAITING -1
#define BLINK 0
#define WINK 1
#define STARTLED 3
#define ANNOYED 4
#define PSYCHO 5
#define DEAD 6
#define REVIVED 7
#define DEFENDER 8

int eyeptr=0;
int mouthptr=0;
int frameidxL=0;
int frameidxR=0;
signed char *eyeanimL;
signed char *eyeanimR;
int eyemax = 0;
int waittick=0;
int state=0;
int blinkidx=0;
int blinkdir=0;
int nextstate=0;  // For queueing user-triggered states
bool updateLR=true;

// transfer buffer for programming the display chip
unsigned char spidata[NUM_PANELS * 2];    // Dedicated to sprite data
unsigned char spicmd[NUM_PANELS * 2]; // Dedicated to commands

// Frame buffer for procedural blink
unsigned char framebufferL[8][2];
unsigned char framebufferR[8][2];

// Standing mountain range for defender - the rest will be generated randomly
unsigned char defender_map[64]={1,3,7,7,3,7,15,31,15,7,15,31,31,15,7,3,7,7,15,31,15,15,7,3,1,1,1,3,7,15,3,3,1,1,0,0,1,3,7,15,15,31,31,15,31,15,7,3,1,0,0,0,1,3,7,3,1,0,0,1,3,7,15,7};

//
//  Animation data
//

//
// Animation sequences
//
// These are indicies into the sprite list, e.g. 0 is fully open, 1 is blank (closed), 8 is the first frame of the startled animation etc.
// If you insert or remove any existing frames, you'll need to bump the numbers along!
// Negative numbers indicate a delay in wait cycles (60ms) rather than a frame number.
// See the comments in the sprite data below for the ID of each frame
//

// NOTE: Animations should always start with the default expression (e.g. 0) as they go back to the first frame in the list when the sequence finishes!

signed char closeeye[] = {0,-50,0}; // Blink is now done procedurally instead of using a complex animation, so just display one frame and wait

signed char startled[] = {0, 3,-15,0};

signed char annoyed[] = {0, 4,-15, 0};

signed char psycho_l[] = {0, 7,-15, 1, -2, 7, -2, 1, -2, 7, -2, 1, -2, 7, -2, 1, -2, 7, -2, 1, -2, 7, -2, 1, -2, 7, -2, 1, 0};

signed char psycho_r[] = {0, 8,-15, 1, -2, 8, -2, 1, -2, 8, -2, 1, -2, 8, -2, 1, -2, 8, -2, 1, -2, 8, -2, 1, -2, 8, -2, 1, 0};

signed char dead[] = {9,-100};  // HACK: this won't go back to normal

signed char defender[] = {0, -5, 0};

//
//  Sprite data
//
// These are  monochrome sprites, 0 is dark, 1 is lit
//
// You could also add extra animation frames and logic, e.g. to roll the eyes
// Or to make them animate a pixel at a time instead of two pixels
//
// Note that these are currently stored in dynamic memory so there's a 2KB limit,
// As of this writing we're using 650 bytes, and each sprite takes 32 bytes.
// Putting the sprites into program memory will give you about 30KB to play with
// but you'd have to modify the software to read them from that address space.
//

// Right eye (facing left)
unsigned char eye[][16] = {
    // Basic open eye (0)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000001,B11110000,
      B00000111,B11111100,
      B00000111,B00000100,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // closed (blank) (1)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // annoyed (2)
    {
      B00000000,B00000000,
      B00001111,B11110000,
      B00001111,B11000000,
      B00001111,B00000000,
      B00001100,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // annoyed (3)
    {
      B00000000,B00000000,
      B00000000,B00110000,
      B00000000,B11110000,
      B00000011,B11110000,
      B00001111,B11110000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // annoyed (4)
    {
      B00000000,B00000000,
      B00000001,B11111100,
      B00000011,B11110000,
      B00000111,B11000000,
      B00001111,B00000000,
      B00111100,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // Init screen L - flipped - (5)
    {
      B00000000,B00000000,
      B00000000,B00000010,
      B00000000,B00000010,
      B00000000,B00000010,
      B00000000,B00000010,
      B00000000,B00000010,
      B00000000,B01111110,
      B00000000,B00000000,
    },    
    // Init screen R (6)
    {
      B00000000,B00000000,
      B00000000,B01111000,
      B00000000,B01000100,
      B00000000,B01111000,
      B00000000,B01000100,
      B00000000,B01000100,
      B00000000,B01000100,
      B00000000,B00000000,
    },    
    // Psycho mode L -flipped - (7)
    {
      B00000000,B10000000,
      B00010000,B10101001,
      B00010000,B10001001,
      B00010000,B10100101,
      B00010000,B10100011,
      B00010000,B10100101,
      B00010111,B10101001,
      B11110111,B10001001,
    },
    // Psycho mode R (8)
    {
      B00000001, B00000000,
      B10010101, B00001000,
      B10010001, B00001000,
      B10100101, B00001000,
      B11000101, B00001000,
      B10100101, B00001000,
      B10010101, B11101000,
      B10010101, B11101111,
    },
    // Dead (9)
    {
      B00001100,B00110000,
      B00000110,B01100000,
      B00000011,B11000000,
      B00000001,B10000000,
      B00000011,B11000000,
      B00000110,B01100000,
      B00001100,B00110000,
      B00000000,B00000000,
    }
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

unsigned char mouth[][32] = {
    // happy (0)
    {
      B00000000,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000100,
      B00000000,B00000000,B00000000,B00001000,
      B00000000,B00000000,B00000000,B00110000,
      B01000000,B00000000,B00000011,B11000000,
      B00100110,B00011100,B11001100,B00000000,
      B00011001,B11100011,B00110000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
    },
    // unhappy (1)
    {
      B00000000,B00000000,B00000000,B00000000,
      B00011001,B11100011,B00110000,B00000000,
      B00100110,B00011100,B11001100,B00000000,
      B01000000,B00000000,B00000011,B11000000,
      B00000000,B00000000,B00000000,B00110000,
      B00000000,B00000000,B00000000,B00001000,
      B00000000,B00000000,B00000000,B00000100,
      B00000000,B00000000,B00000000,B00000000,
    },
    // surprised (2)
    {
      B00000000,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
      B00001100,B00000000,B00000000,B00000000,
      B00010010,B00000000,B00000000,B00000000,
      B00010010,B00000000,B00000000,B00000000,
      B00001100,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
    },
    // defender DB (3)
    {
      B00000000,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
      B00001100,B00000000,B00000000,B00000000,
      B00010010,B00000000,B00000000,B00000000,
      B00010010,B00000000,B00000000,B00000000,
      B00001100,B00000000,B00000000,B00000000,
      B00000000,B00000000,B00000000,B00000000,
    },
  };

//
//  State structure
//

struct STATES {
  char id;
  signed char *animL;
  signed char *animR;
  unsigned char animlen;
  unsigned char mouth;
  char pin;
};

// Add any new animation triggers here

struct STATES states[] = {
{BLINK,     closeeye,   closeeye,     sizeof(closeeye), 0, 0},
{STARTLED,  startled,   startled,     sizeof(startled), 2, STARTLED_PIN},
{ANNOYED,   annoyed,    annoyed,      sizeof(annoyed),  1, ANNOYED_PIN},
{PSYCHO,    psycho_l,   psycho_r,     sizeof(psycho_l), 0, 0},
{DEAD,      dead,       dead,         sizeof(dead),     1, KILL_PIN},
{REVIVED,   startled,   startled,     sizeof(startled), 2, REVIVE_PIN},
{DEFENDER,  defender,   defender,     sizeof(defender), 3, 0},
// DO NOT REMOVE THIS LAST LINE!
{0,         NULL,       NULL,         0,                0, 0}  
};


//
//  Here's the actual implementation
//

void setup() {
  pinMode(CS_PIN,OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Init pins for states
  for(int ctr=0;states[ctr].animL;ctr++) {
    if(states[ctr].pin) {
      pinMode(states[ctr].pin, INPUT_PULLUP);
    }
  }

  // Set up data transfers
  SPI.begin();
  SPI.beginTransaction( SPISettings(16000000, MSBFIRST, SPI_MODE3 ) );

  initPanels(BRIGHTNESS);
 
  randomSeed(0);  

  eyeanimL = &closeeye[0];
  eyeanimR = &closeeye[0];
  eyemax = sizeof(closeeye);
  state = BLINK;
  mouthptr = 0;

  // Diagnostics
  getSpriteL(eye[5], 0);
  getSpriteR(eye[6], 0);
  drawEyes();

  delay(2000);

  // Initial draw
  getSpriteL(&eye[0][0], 0);
  getSpriteR(&eye[0][0], 0);
  drawEyes();

  // HACK
#ifdef DEAD_PROTO  
  state = DEAD;
  nextstate = DEAD;
  getNextAnim();
#endif
}

void initPanels(int brightness) {
  // Initialise the panels

  setRegister(CMD_TEST,0);
  setRegister(CMD_DECODE,0);  // Disable BCD decoder so we can sent sprite data instead
  setRegister(CMD_INTENSITY,brightness);
  setRegister(CMD_SCANLIMIT,7);
  setRegister(CMD_SHUTDOWN,1);   // 0 turns it off, 1 turns it on
}

void loop() {
   static int flashtimer=128;
   static int defendertimer=1024;
   flashtimer--;
   if(flashtimer < 0) {
    flashtimer = 128;
   }

#ifdef ENABLE_DEFENDER
    if(state == DEFENDER) {
      defendertimer--;
      if(defendertimer < 1) {

        initPanels(BRIGHTNESS); // Reboot the panels as this sometimes crashes one of them
        
        getNextAnim();
        defendertimer = 512;
      }
      nextstate = BLINK;
      wait(FRAME_IN_MS,false); // Can't interrupt a blink
      // Disable eye for power saving
      getSpriteL(&eye[0][0], 8);
      getSpriteR(&eye[0][0], 8);
      update_defender();
      updateLR=true;
      initPanels(BRIGHTNESS); // Reboot the panels every frame, this is a horrible workaround (NB: we could use this to modulate the brightness by voice)
      drawEyes();
      return;
    }
#endif
  
   // Draw the sprites
   getSpriteL(&eye[frameidxL][0], state == WINK ? 0 : blinkidx);
   getSpriteR(&eye[frameidxR][0], blinkidx);
   drawEyes();

  // If we're idling, count down
  if(waittick > 0) {
    if(state == BLINK || state == WINK) {
      wait(FRAME_IN_MS,false); // Can't interrupt a blink
    } else {
      wait(WAIT_IN_MS,true); // Can interrupt
    }
    waittick--;
  } else {
    if(state == WAITING) {
      getNextAnim();
    }
    
    // Otherwise, update the animation
    updateLR=true;
    wait(FRAME_IN_MS,false);

    if(state == PSYCHO) {
      digitalWrite(LED_BUILTIN, flashtimer & 8);
    }

    eyeptr++;
    if(eyeptr >= eyemax || nextstate) {
      // If we've hit the end, go back to the start and wait
      eyeptr=0;
      // Wait between 5-250 cycles before blinking again
      waittick = random(MIN_DELAY,MAX_DELAY);
      state = WAITING;
    }
    
    // Negative is pause in cycles
    if(eyeanimL[eyeptr] < 0) {
      waittick = -eyeanimL[eyeptr];
      return;
    }

    frameidxL=eyeanimL[eyeptr];
    frameidxR=eyeanimR[eyeptr];
  }
  
  // Handle blinking
  if(state == BLINK || state == WINK) {
    blinkidx += blinkdir;
    if(blinkidx > 10) {
      blinkdir = -1;
    }
    if(blinkidx < 1) {
      blinkidx=0;
      blinkdir=0;
      // Wait between 5-250 cycles before blinking again
      waittick = random(MIN_DELAY,MAX_DELAY);
      state = WAITING;
    }
    
    updateLR=true;
  }
  
  
}

//
//  Pick the animation to roll, which may have been cued up in response to a GPIO pin
//

void getNextAnim() {
  int ctr;

  // Stay dead
  if((state == DEAD || nextstate == DEAD) && nextstate != REVIVED) {
    state = DEAD;
  }

  eyeptr=0;
  mouthptr=0;
  state = nextstate;

  for(ctr=0;states[ctr].animL;ctr++) {
    if(states[ctr].id == nextstate) {
      eyeanimL = states[ctr].animL;
      eyeanimR = states[ctr].animR;
      eyemax = states[ctr].animlen;
      mouthptr = states[ctr].mouth;
      break;
    }
  }

  if(state == DEAD) {
    // Lie down and stop breathing at once!
    digitalWrite(LED_BUILTIN, HIGH); // Steady state
    return;
  }

  if(state == REVIVED) {
    digitalWrite(LED_BUILTIN, LOW); // Alive again
  }
  
  nextstate = BLINK;

  if(state == BLINK) {

#ifdef ENABLE_PSYCHO
    ctr = random(1,25);  // 1 in 25 chance of him doing the Kill thing
    if(ctr == 1) {
      state = PSYCHO;
      eyeanimL = psycho_l;
      eyeanimR = psycho_r;
      eyemax = sizeof(psycho_l); // Must be same size!
      return;
    }
#endif

#ifdef ENABLE_DEFENDER
    ctr = random(1,5);  // 1 in 10 chance of him playing defender
    if(ctr == 1) {
      nextstate = DEFENDER;
      return;
    }
#endif
    
    // 1 in 10 chance of him winking
    ctr = random(1,10);
    if(ctr == 1) {
      state = WINK;
    }
    blinkdir=1;
  }
}

//
//  Get the requested animation frame and apply a blink effect if necessary
//

void getSpriteL(unsigned char *ptr, int blinkpos) {
  memcpy(framebufferL,ptr,16);
  if(blinkpos > 0) {
    if(blinkpos > 8) {
      blinkpos = 8;
    }
    memset(framebufferL,0,blinkpos * 2);
  }
}

void getSpriteR(unsigned char *ptr, int blinkpos) {
  memcpy(framebufferR,ptr,16);
  if(blinkpos > 0) {
    if(blinkpos > 8) {
      blinkpos = 8;
    }
    memset(framebufferR,0,blinkpos * 2);
  }
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
//  Thanks to the cable routing the panels are upside-down so we only need to flip vertically
//

void drawEyes() {
  unsigned char *ptr_le = &framebufferL[0][0];
  unsigned char *ptr_re = &framebufferR[0][0];
  unsigned char *ptr_m = &mouth[mouthptr][0];
  unsigned char *ptr_n = &nostril[0];

  unsigned char *old_m,*old_n;
  int pos;

  if(!updateLR) {
    return;
  }

  // With Quirk's current wiring the panels are both upside-down.
  // If you need to change this, edit how 'pos' is set below:

  for( int row=0;row<8; row++) {

#ifdef FLIP_L_VERTICAL
    pos = 8-row;
#else
    pos = row+1;
#endif

    old_m=ptr_m;
    old_n=ptr_n;
    
    writeData(LEFT_EYE_1,pos,GET_LEFT(*ptr_le++));
    writeData(LEFT_EYE_2,pos,GET_LEFT(*ptr_le++));
    writeData(LEFT_MOUTH_1,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_MOUTH_2,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_MOUTH_3,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_MOUTH_4,pos,GET_LEFT(*ptr_m++));
    writeData(LEFT_NOSTRIL,pos,GET_LEFT(*ptr_n++));

#ifdef FLIP_R_VERTICAL
    pos = 8-row;
#else
    pos = row+1;
#endif

    ptr_m=old_m;
    ptr_n=old_n;

    writeData(RIGHT_EYE_1,pos,GET_RIGHT(*ptr_re++));
    writeData(RIGHT_EYE_2,pos,GET_RIGHT(*ptr_re++));
    writeData(RIGHT_MOUTH_1,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_MOUTH_2,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_MOUTH_3,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_MOUTH_4,pos,GET_RIGHT(*ptr_m++));
    writeData(RIGHT_NOSTRIL,pos,GET_RIGHT(*ptr_n++));
    
    blit();
  }
  
  // Don't send again until the frame changes
  updateLR=false;
  
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


void voiceDetect() {
  static int brightcount=0;
  static int lastbright = -1;
  int newbright;

  #ifdef VOICE_DETECTOR
    bool bright = !(analogRead(VOICE_PIN) > ADC_THRESHOLD);
  #else
    bool bright = false;
  #endif

  if(bright) {
    brightcount=50;
  }

  if(brightcount > 0) {
      brightcount--;
  }

  newbright = brightcount > 0?VOICE_BRIGHT:BRIGHTNESS;
  if(newbright != lastbright) {
    setRegister(CMD_INTENSITY,newbright);
    lastbright = newbright;
  }
}


//
//  Wait, and cue up a reaction if we detect one via GPIO
//

void wait(int ms, bool interruptable) {
  for(int ctr=0;ctr<ms;ctr++) {
    delay(1);
    voiceDetect();
    if(state == WAITING) {
      for(int ctr2=0;states[ctr2].animL;ctr2++) {
        if(states[ctr2].pin) {
          if(checkExpression(states[ctr2].pin)) {
            nextstate = states[ctr2].id;
            if(interruptable) {
              waittick=0;
              return;
            }
          }
        }
      }
    }
  }
}

#ifdef ENABLE_DEFENDER

void update_defender() {
  unsigned char *defender_fb = &mouth[3][0];
  static int scrollpos;
  static boolean divider;
  static int bulletx=-1,bullety;
  int ypos;
  divider = !divider;
  if(divider) {
    update_map(&defender_map[0]);
  }

  render_heightfield(&defender_map[0], &defender_fb[0]);
  render_heightfield(&defender_map[8], &defender_fb[1]);
  render_heightfield(&defender_map[16], &defender_fb[2]);
  render_heightfield(&defender_map[24], &defender_fb[3]);

  ypos = get_shippos(8);
  add_ship(&defender_fb[1], ypos);

  if(bulletx < 0) {
    if(random(1,20) == 1) {
      bulletx=12;
      bullety=ypos+1;
    }
  } else {
    if(!add_bullet(&defender_fb[0],bulletx,bullety)) {
      bulletx = -1; // Hit something
      return;
    }
    bulletx++;
    if(bulletx > 31) {
      bulletx=-1;
    }
  }
}

// Scroll the map along and randomly generate new mountains

void update_map(unsigned char *map) {
  static int dir=1;
  static int y=0;
  // Shuffle it along
  for(int ctr=0;ctr<31;ctr++) {
    map[ctr]=map[ctr+1];
  }
  
  map[31]=1 << y;
  map[31]--;

  if(random(1,3) != 1) {
    y += dir;
  }

  if(y>5) {
    if(random(1,3) == 1) {
      dir=-1;
    }
  }
  
  if(y>6) {
    y=5;
    dir=-1;
  }
  if(y<0) y=0;

  if(random(1,4) == 1) {
    dir = -dir;
  }
}


void render_heightfield(unsigned char *in, unsigned char *out) {
  unsigned char a,b,c,d,e,f,g,h;
  a=*in++;  b=*in++;  c=*in++;  d=*in++;
  e=*in++;  f=*in++;  g=*in++;  h=*in++;

  for(int y=0;y<8;y++) {
    out[y<<2]=(a&128)|((b&128)>>1)|((c&128)>>2)|((d&128)>>3)|((e&128)>>4)|((f&128)>>5)|((g&128)>>6)|((h&128)>>7);
    a<<=1;    b<<=1;    c<<=1;    d<<=1;
    e<<=1;    f<<=1;    g<<=1;    h<<=1;
  }
}

void add_ship(unsigned char *fb, int y) {
  fb += (y<<2);
  *fb |= 0x80;  // 1 for other way
  fb += 4;
  *fb |= 0xe0;  // 6 for other way
}

bool add_bullet(unsigned char *fb, int x, int y) {
int panel = x>>3;
x = x&7;

fb += (y<<2);

x = 128>>x;

if(fb[panel] & x) {
  return false; // Hit something
}
fb[panel] |= x;

return true;
}

int get_maxheight(int slice) {
  if(defender_map[slice] & 128) return 8;
  if(defender_map[slice] & 64)  return 7;
  if(defender_map[slice] & 32)  return 6;
  if(defender_map[slice] & 16)  return 5;
  if(defender_map[slice] & 8)  return 4;
  if(defender_map[slice] & 4)  return 3;
  if(defender_map[slice] & 2)  return 2;
  if(defender_map[slice] & 1)  return 1;
  return 0;
}

int get_shippos(int slice) {
  int max=0,c;
  max=get_maxheight(slice);
  c=get_maxheight(slice+1);
  if(c>max) {
    max=c;
  }
  c=get_maxheight(slice+2);
  if(c>max) {
    max=c;
  }

  c = (8-max)-4;
  if(c<0) {
    c=0;
  }
  return c;
}

#endif

//
//  Default handler for Arduino, can be replaced for other platforms
//

#ifndef CUSTOM_EXPRESSION_HANDLER
bool checkExpression(int pin) {
    return (digitalRead(pin) == LOW);
}
#endif
