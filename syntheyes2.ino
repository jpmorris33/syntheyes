//
//  Synth Eyes for Arduino
//  V2.1 - With sprite-flipping and reactions
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

#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds (30 default)
#define WAIT_IN_MS  60  // Delay per tick in milliseconds when waiting to blink again (60 default)
#define BRIGHTNESS  12  // Brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)
#define CS_PIN 10       // Chip select pin

#define STARTLED_PIN 7
#define EYEROLL_PIN 8

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

void drawEye(unsigned char *ptr);
void sendData(int addr, byte opcode, byte data);
void wait(int ms, bool interruptable);

// System state variables

#define WAITING -1
#define BLINK 0
#define WINK 1
#define ROLLEYE 2
#define STARTLED 3

int eyeptr=0;
int lFrame=0;
int rFrame=0;
char *eyeanim;
int eyemax = 0;
int waittick=0;
int state=0;
int nextstate=0;  // For queueing user-triggered states

// transfer buffer for programming the display chip
unsigned char spidata[16];

//
//  Animation data
//

// Animation sequence for closing eye
char closeeye[] = {0,1,2,3,4,5,5,5,5,5,5,4,3,2,1,0};

char rolleye[] = {0,6,6,7,7,8,8,9,9,10,10,11,11,-20,11,11,10,10,9,9,8,8,7,7,6,6,0};

char startled[] = {0,12,13,-30,13,12,0};

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
// As of this writing we're using 438 bytes, and each sprite takes 32 bytes.
// Putting the sprites into program memory will give you about 30KB to play with
// but you'd have to modify the software to read them from that address space.
//

// Right eye (facing left)
unsigned char eye[][32] = {
    // Fully open (0)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x0f,0x80,
      0x1f,0xc0,

      0x3f,0xe0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x73,0xf0,
      0x61,0xf0,
      0x61,0xf0,
      0x61,0xf0,
    },
    // closing 1 (1)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,

      0x3f,0xe0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x73,0xf0,
      0x61,0xf0,
      0x61,0xf0,
      0x61,0xf0,
    },
    // closing 2 (2)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,

      0x00,0x00,
      0x00,0x00,
      0x7f,0xf0,
      0x7f,0xf0,
      0x73,0xf0,
      0x61,0xf0,
      0x61,0xf0,
      0x61,0xf0,
    },
    // closing 3 (3)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,

      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x73,0xf0,
      0x61,0xf0,
      0x61,0xf0,
      0x61,0xf0,
    },
    // closing 4 (4)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,

      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x61,0xf0,
      0x61,0xf0,
    },
    // closed (blank) (5)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,

      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
    },
    // roll eye 1 (6)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x0f,0x80,
      0x1f,0xc0,
      0x3f,0xe0,
      0x7f,0xf0,

      0x7f,0xf0,
      0x7f,0xf0,
      0x73,0xf0,
      0x61,0xf0,
      0x61,0xf0,
      0x61,0xf0,
      0x73,0xf0,
      0x7f,0xf0,
    },
    // roll eye 2 (7)
    {
      0x00,0x00,
      0x00,0x00,
      0x0f,0x80,
      0x1f,0xc0,
      0x3f,0xe0,
      0x7f,0xf0,
      0x73,0xf0,
      0x61,0xf0,

      0x61,0xf0,
      0x61,0xf0,
      0x73,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x3f,0xe0,
      0x1f,0xc0,
    },
    // roll eye 3 (8)
    {
      0x07,0xc0,
      0x1f,0xe0,
      0x3f,0xf0,
      0x3c,0xf8,
      0x38,0x78,
      0x38,0x78,
      0x38,0x78,
      0x3c,0xf8,

      0x3f,0xf8,
      0x3f,0xf8,
      0x3f,0xf8,
      0x3f,0xf8,
      0x1f,0xf0,
      0x0f,0xe0,
      0x07,0xc0,
      0x00,0x00,
    },
    // roll eye 4 (9)
    {
      0x0f,0xf8,
      0x1f,0x3c,
      0x1e,0x1c,
      0x1e,0x1c,
      0x1e,0x1c,
      0x1f,0x3c,
      0x1f,0xfc,
      0x1f,0xfc,

      0x1f,0xfc,
      0x1f,0xfc,
      0x0f,0xf8,
      0x07,0xf0,
      0x03,0xe0,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
    },
    // roll eye 5 (10)
    {
      0x0f,0xce,
      0x0f,0x86,
      0x0f,0x86,
      0x0f,0x86,
      0x0f,0xce,
      0x0f,0xfe,
      0x0f,0xfe,

      0x0f,0xfe,
      0x0f,0xfe,
      0x07,0xfc,
      0x03,0xf8,
      0x01,0xf0,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
    },
    // roll eye 6 (11)
    {
      0x07,0xc3,
      0x07,0xc3,
      0x07,0xc3,
      0x07,0xe7,
      0x07,0xff,
      0x07,0xff,
      0x07,0xff,
      0x03,0xfe,

      0x01,0xfc,
      0x00,0xf8,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
    },
    // startled 1 (12)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x0f,0x80,
      0x1f,0xc0,
      
      0x3f,0xe0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x73,0xf0,
      0x73,0xf0,
      0x73,0xf0,
    },
    // startled 2 (13)
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x0f,0x80,
      0x1f,0xc0,
      
      0x3f,0xe0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x7f,0xf0,
      0x73,0xf0,
      0x73,0xf0,
      0x7f,0xf0,
    },
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
  pinMode(EYEROLL_PIN,INPUT_PULLUP);
  pinMode(STARTLED_PIN,INPUT_PULLUP);
  digitalWrite(CS_PIN, LOW);
  
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
}

void loop() {
  // Draw the sprites
  drawEyeR(&eye[rFrame][0]);
  drawEyeL(&eye[lFrame][0]);

  // If we're idling, count down
  if(waittick > 0) {
    wait(WAIT_IN_MS,true); // Can interrupt
    waittick--;
  } else {
    if(state == WAITING) {
      getNextAnim();
    }
    
    // Otherwise, update the animation
    wait(FRAME_IN_MS,false);
    
    eyeptr++;
    if(eyeptr >= eyemax || nextstate) {
      // If we've hit the end, go back to the start and wait
      eyeptr=0;
      // Wait between 5-250 cycles before blinking again
      waittick = random(5,250);
      state = WAITING;
  }

    // Negative is pause in cycles
    if(eyeanim[eyeptr] < 0) {
      waittick = -eyeanim[eyeptr];
      return;
    }

    // By default, have both eyes animate
    lFrame=rFrame=eyeanim[eyeptr];

    // If we're winking, keep the left eye open
    if(state == WINK) {
      lFrame=eyeanim[0];
    }

  }
  
}

//
//  Pick the animation to roll, which may have been cued up in response to a GPIO pin
//

void getNextAnim() {
  int temp;

  eyeptr=0;
  state = nextstate;

  switch(nextstate) {
    case ROLLEYE:
      eyeanim = rolleye;
      eyemax = sizeof(rolleye);
    break;

    case STARTLED:
      eyeanim = startled;
      eyemax = sizeof(startled);
    break;
    
    default:
      eyeanim = closeeye;
      eyemax = sizeof(closeeye);
    break;
    
  }
  nextstate = BLINK;

  if(state == BLINK) {
    // 1 in 5 chance of him winking
    temp = random(1,5);
    if(temp == 1 && state ) {
      state = WINK;
    }
  }
}

//
//  Draw the sprite on the 'right' display matrix
//

void drawEyeR(unsigned char *ptr) {
  for( int row=0;row<8; row++) {
    sendData(RPANEL_TL,row+1,*ptr++);
    sendData(RPANEL_TR,row+1,*ptr++);
  }
  for( int row=0;row<8; row++) {
    sendData(RPANEL_BL,row+1,*ptr++);
    sendData(RPANEL_BR,row+1,*ptr++);
  }
}

//
//  Draw the mirrored sprite on the 'left' matrix (note the L&R panels are swapped)
//

void drawEyeL(unsigned char *ptr) {
  for( int row=0;row<8; row++) {
    sendData(LPANEL_TR,row+1,pgm_read_byte(&(reverse[*ptr++])));
    sendData(LPANEL_TL,row+1,pgm_read_byte(&(reverse[*ptr++])));
  }
  for( int row=0;row<8; row++) {
    sendData(LPANEL_BR,row+1,pgm_read_byte(&(reverse[*ptr++])));
    sendData(LPANEL_BL,row+1,pgm_read_byte(&(reverse[*ptr++])));
  }
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
      if(digitalRead(STARTLED_PIN) == LOW) {
        nextstate = STARTLED;
        if(interruptable) {
          waittick=0;
          return;
        }
      }
      
      if(digitalRead(EYEROLL_PIN) == LOW) {
        nextstate = ROLLEYE;
        if(interruptable) {
          waittick=0;
          return;
        }
      }
    }
  }
}
