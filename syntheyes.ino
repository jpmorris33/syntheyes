//
//  Synth Eyes for Arduino
//  V3.0.1 - Bug fix
//  V3.0.0 - Restructure to use procedural blinking
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


#include <SPI.h>

// Configurables, adjust to taste

#define BRIGHTNESS  2  // Brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)
#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds (20 default)
#define WAIT_IN_MS  60  // Delay per tick in milliseconds when waiting to blink again (60 default)
#define MIN_DELAY    5   // Minimum delay between blinks
#define MAX_DELAY    250 // Maximum delay between blinks

#define CS_PIN 10       // Chip select pin

#ifndef OVERRIDE_PINS
	#define STARTLED_PIN 7
	#define EYEROLL_PIN 8
	#define ANNOYED_PIN 6
#endif

//
//  Panel positions for each corner of the sprite
//
//  If the sprite appears garbled, with the wrong displays showing each corner
//  because your panels are wired differently to mine, you may need to adjust
//  the ID of each panel here, e.g. 0-3 instead of 3-0
//

#define RPANEL_TL  3
#define RPANEL_TR  2
#define RPANEL_BL  1
#define RPANEL_BR  0
#define LPANEL_TL  7
#define LPANEL_TR  6
#define LPANEL_BL  5
#define LPANEL_BR  4

// System constants, you probably don't want to touch these

#define CMD_TEST      0x0f
#define CMD_INTENSITY 0x0a
#define CMD_SCANLIMIT 0x0b
#define CMD_DECODE    0x09
#define CMD_SHUTDOWN  0x0C

// Functions

void drawEyeL();
void drawEyeR();
void getSprite(unsigned char *ptr, int blinkpos);
void sendData(int addr, byte opcode, byte data);
void wait(int ms, bool interruptable);
void getNextAnim();
bool checkExpression(int pin);

// System state variables

#define WAITING -1
#define BLINK 0
#define WINK 1
#define ROLLEYE 2
#define STARTLED 3
#define ANNOYED 4

int eyeptr=0;
int frameidx=0;
signed char *eyeanim;
int eyemax = 0;
int waittick=0;
int state=0;
int blinkidx=0;
int blinkdir=0;
int nextstate=0;  // For queueing user-triggered states
bool updateL=true;
bool updateR=true;

// transfer buffer for programming the display chip
unsigned char spidata[16];

// Frame buffer for procedural blink
unsigned char framebuffer[16][2];

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

signed char closeeye[] = {0,-50,0}; // Blink is now done procedurally instead of using a complex animation, so just display one frame and wait

signed char rolleye[] = {0,2,2,3,3,4,4,5,5,6,6,7,7,-20,7,7,6,6,5,5,4,4,3,3,2,2,0};

signed char startled[] = {0,8,9,-30,9,8,0};

signed char annoyed[] = {0,10,11,-30,11,10,0};

//
//  Sprite data
//
// These are 16x16 monochrome sprites, 0 is dark, 1 is lit
// The first byte is the top-left corner, second byte is the top-right corner.
// The third byte is the left 8 pixels of the second row, fourth is the right 8 pixels and so on.
//
// If customising the sprites, you may find it easier to express these in binary,
// e.g.  B00010000, B00111000
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
unsigned char eye[][32] = {
    // Basic open eye (0)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00001111,B10000000,
      B00011111,B11000000,
      B00111111,B11100000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
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
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // roll eye 1 (2)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00001111,B10000000,
      B00011111,B11000000,
      B00111111,B11100000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01110011,B11110000,
      B01111111,B11110000,
    },
    // roll eye 2 (3)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00001111,B10000000,
      B00011111,B11000000,
      B00111111,B11100000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01110011,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B00111111,B11100000,
      B00011111,B11000000,
    },
    // roll eye 3 (4)
    {
      B00000111,B11000000,
      B00011111,B11100000,
      B00111111,B11110000,
      B00111100,B11111000,
      B00111000,B01111000,
      B00111000,B01111000,
      B00111000,B01111000,
      B00111100,B11111000,
      B00111111,B11111000,
      B00111111,B11111000,
      B00111111,B11111000,
      B00111111,B11111000,
      B00011111,B11110000,
      B00001111,B11100000,
      B00000111,B11000000,
      B00000000,B00000000,
    },
    // roll eye 4 (5)
    {
      B00001111,B11111000,
      B00011111,B00111100,
      B00011110,B00011100,
      B00011110,B00011100,
      B00011110,B00011100,
      B00011111,B00111100,
      B00011111,B11111100,
      B00011111,B11111100,
      B00011111,B11111100,
      B00011111,B11111100,
      B00001111,B11111000,
      B00000111,B11110000,
      B00000011,B11100000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // roll eye 5 (6)
    {
      B00001111,B11001110,
      B00001111,B10000110,
      B00001111,B10000110,
      B00001111,B10000110,
      B00001111,B11001110,
      B00001111,B11111110,
      B00001111,B11111110,
      B00001111,B11111110,
      B00001111,B11111110,
      B00000111,B11111100,
      B00000011,B11111000,
      B00000001,B11110000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // roll eye 6 (7)
    {
      B00000111,B11000011,
      B00000111,B11000011,
      B00000111,B11000011,
      B00000111,B11100111,
      B00000111,B11111111,
      B00000111,B11111111,
      B00000111,B11111111,
      B00000011,B11111110,
      B00000001,B11111100,
      B00000000,B11111000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // startled 1 (8)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00001111,B10000000,
      B00011111,B11000000,
      B00111111,B11100000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01110011,B11110000,
      B01110011,B11110000,
    },
    // startled 2 (9)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00001111,B10000000,
      B00011111,B11000000,
      B00111111,B11100000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01110011,B11110000,
      B01111111,B11110000,
    },
  // Annoyed 1 (10)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000001,B10000000,
      B00000111,B11000000,
      B00011111,B11100000,
      B00111111,B11110000,
      B01111111,B11110000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
    },
// Annoyed 2 (11)
    {      
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B01000000,
      B00000001,B11100000,
      B00000111,B11110000,
      B00011111,B11110000,
      B01111111,B11110000,
      B01110011,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
      B01100001,B11110000,
    }
  };

//
//  State structure
//

struct STATES {
  char id;
  signed char *anim;
  unsigned char animlen;
  char pin;
};

// Add any new animation triggers here

struct STATES states[] = {
{BLINK,     closeeye,    sizeof(closeeye), 0},
{WINK,      closeeye,    sizeof(closeeye), 0},
{ROLLEYE,   rolleye,     sizeof(rolleye),  EYEROLL_PIN},
{STARTLED,  startled,    sizeof(startled), STARTLED_PIN},
{ANNOYED,   annoyed,     sizeof(annoyed),  ANNOYED_PIN},
// DO NOT REMOVE THIS LAST LINE!
{0,         NULL,        0,                0}  
};


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

//
//  Here's the actual implementation
//

void setup() {
  pinMode(CS_PIN,OUTPUT);
  digitalWrite(CS_PIN, LOW);

  // Init pins for states
  for(int ctr=0;states[ctr].anim;ctr++) {
    if(states[ctr].pin) {
      pinMode(states[ctr].pin, INPUT_PULLUP);
    }
  }
  
  // Set up data transfers
  SPI.begin();
  SPI.beginTransaction( SPISettings(16000000, MSBFIRST, SPI_MODE3 ) );
  digitalWrite(CS_PIN, HIGH);
  
  // Initialise the panels
  for(int panel=0;panel<8;panel++) {
      sendData(panel, CMD_TEST,0);
      sendData(panel, CMD_DECODE,0);    // Disable BCD decoder so we can sent sprite data instead
      sendData(panel, CMD_INTENSITY,BRIGHTNESS);
      sendData(panel, CMD_SCANLIMIT,7);
      sendData(panel, CMD_SHUTDOWN,1);  // 0 turns it off, 1 turns it on
  }
  
  randomSeed(0);  

  eyeanim = &closeeye[0];
  eyemax = sizeof(closeeye);
  state = BLINK;

  // Initial draw
  getSprite(&eye[frameidx][0], 0);
  drawEyeR();
  drawEyeL();
}

void loop() {
    // Draw the sprites
    getSprite(&eye[frameidx][0], blinkidx);
    drawEyeR();
    getSprite(&eye[frameidx][0], state == WINK ? 0 : blinkidx);
    drawEyeL();

  // If we're idling, count down
  if(waittick > 0) {
    if(state == BLINK || state == WINK) {
      wait(FRAME_IN_MS/2,false); // Can't interrupt a blink and it runs twice as fast as usual animations
    } else {
      wait(WAIT_IN_MS,true); // Can interrupt
    }
    waittick--;
  } else {
    if(state == WAITING) {
      getNextAnim();
    }
    
    // Otherwise, update the animation
    updateL=updateR=true;
    wait(FRAME_IN_MS,false);
    
    eyeptr++;
    if(eyeptr >= eyemax || nextstate) {
      // If we've hit the end, go back to the start and wait
      eyeptr=0;
      // Wait between 5-250 cycles before blinking again
      waittick = random(MIN_DELAY,MAX_DELAY);
      state = WAITING;
    }

    // Negative is pause in cycles
    if(eyeanim[eyeptr] < 0) {
      waittick = -eyeanim[eyeptr];
      return;
    }

    frameidx=eyeanim[eyeptr];
  }
  
  // Handle blinking
  if(state == BLINK || state == WINK) {
    blinkidx += blinkdir;
    if(blinkidx > 20) {
      blinkdir = -1;
    }
    if(blinkidx < 1) {
      blinkidx=0;
      blinkdir=0;
      // Wait between 5-250 cycles before blinking again
      waittick = random(MIN_DELAY,MAX_DELAY);
      state = WAITING;
    }
    
    updateL=updateR=true;
  }
  
  
}

//
//  Pick the animation to roll, which may have been cued up in response to a GPIO pin
//

void getNextAnim() {
  int ctr;

  eyeptr=0;
  state = nextstate;

  for(ctr=0;states[ctr].anim;ctr++) {
    if(states[ctr].id == nextstate) {
      eyeanim = states[ctr].anim;
      eyemax = states[ctr].animlen;
      break;
    }
  }
  nextstate = BLINK;

  if(state == BLINK) {
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

void getSprite(unsigned char *ptr, int blinkpos) {
  memcpy(framebuffer,ptr,32);
  if(blinkpos > 0) {
    if(blinkpos > 16) {
      blinkpos = 16;
    }
    memset(framebuffer,0,blinkpos * 2);
  }
}


//
//  Draw the sprite on the 'right' display matrix
//

void drawEyeR() {
  unsigned char *ptr = &framebuffer[0][0];

  if(!updateR) {
    return;
  }
  
  for( int row=0;row<8; row++) {
    sendData(RPANEL_TL,row+1,*ptr++);
    sendData(RPANEL_TR,row+1,*ptr++);
  }
  for( int row=0;row<8; row++) {
    sendData(RPANEL_BL,row+1,*ptr++);
    sendData(RPANEL_BR,row+1,*ptr++);
  }

  // Don't send again until the frame changes
  updateR=false;
  
}

//
//  Draw the mirrored sprite on the 'left' matrix (note the L&R panels are swapped)
//

void drawEyeL() {
  unsigned char *ptr = &framebuffer[0][0];

  if(!updateL) {
    return;
  }
  
  for( int row=0;row<8; row++) {
    sendData(LPANEL_TR,row+1,pgm_read_byte(&(reverse[*ptr++])));
    sendData(LPANEL_TL,row+1,pgm_read_byte(&(reverse[*ptr++])));
  }
  for( int row=0;row<8; row++) {
    sendData(LPANEL_BR,row+1,pgm_read_byte(&(reverse[*ptr++])));
    sendData(LPANEL_BL,row+1,pgm_read_byte(&(reverse[*ptr++])));
  }

  // Don't send again until the frame changes
  updateL=false;
  
}

//
//  Send a command or data to the display chip
//

void sendData(int addr, byte opcode, byte data) {
  int offset=addr*2;
  memset(spidata,0,16);
  
  // Data is put into the array in reverse order
  // because the chip wants it sent that way
  spidata[15-(offset+1)]=opcode;
  spidata[15-offset]=data;

  // Blit it to the display
  digitalWrite(CS_PIN,LOW);
  SPI.transfer(spidata,16);
  digitalWrite(CS_PIN,HIGH);
}

//
//  Wait, and cue up a reaction if we detect one via GPIO
//

void wait(int ms, bool interruptable) {
  for(int ctr=0;ctr<ms;ctr++) {
    delay(1);
    if(state == WAITING) {
      for(int ctr2=0;states[ctr2].anim;ctr2++) {
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

//
//  Default handler for Arduino, can be replaced for other platfirms
//

#ifndef CUSTOM_EXPRESSION_HANDLER
bool checkExpression(int pin) {
    return (digitalRead(pin) == LOW);
}
#endif
