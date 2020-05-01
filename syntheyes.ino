//
//  Synth Eyes for Arduino
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

#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds
#define WAIT_IN_MS  50  // Delay per tick in milliseconds when waiting to blink again
#define BRIGHTNESS  2   // Brightness from 0-15.  You may need to adjust this
#define CS_PIN 10       // Chip select pin

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
void clearPanels();

// System state variables

int eyeptr=0;
int eyeptrL=0;
int eyeptrR=0;
int waittick=0;
int wink=0;

// transfer buffer for programming the display chip
unsigned char spidata[16];

//
//  Animation data
//

// Animation sequence for closing eye
char eyeanim[] = {0,1,2,3,4,5,5,5,5,4,3,2,1,0};


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

// Right eye
unsigned char eye_r[6][32] = {
    // Fully open
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
    // closing 1
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
    // closing 2
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
    // closing 3
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
    // closing 4
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
    // closed (blank)
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
  };

// Sprite data for left eye (same, but mirrored)
unsigned char eye_l[6][32] = {
    // Fully open
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x01,0xf0,
      0x03,0xf8,

      0x07,0xfc,
      0x0f,0xfe,
      0x0f,0xfe,
      0x0f,0xfe,
      0x0f,0xce,
      0x0f,0x86,
      0x0f,0x86,
      0x0f,0x86,
    },
    // closing 1
    {
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,
      0x00,0x00,

      0x07,0xfc,
      0x0f,0xfe,
      0x0f,0xfe,
      0x0f,0xfe,
      0x0f,0xce,
      0x0f,0x86,
      0x0f,0x86,
      0x0f,0x86,
    },
    // closing 2
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
      0x0f,0xfe,
      0x0f,0xfe,
      0x0f,0xce,
      0x0f,0x86,
      0x0f,0x86,
      0x0f,0x86,
    },
    // closing 3
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
      0x0f,0xce,
      0x0f,0x86,
      0x0f,0x86,
      0x0f,0x86,
    },
    // closing 4
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
      0x0f,0x86,
      0x0f,0x86,
    },
    // closed (blank)
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
  };


//
//  Here's the actual implementation
//

void setup() {
  pinMode(CS_PIN,OUTPUT);
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
  
  // Make sure they're blank
  clearPanels();
  
  randomSeed(0);  
}

void loop() {
  // Draw the sprites
  drawEyeR(&eye_r[eyeanim[eyeptrR]][0]);
  drawEyeL(&eye_l[eyeanim[eyeptrL]][0]);

  // If we're idling, count down
  if(waittick > 0) {
    delay(WAIT_IN_MS);
    waittick--;
  } else {
    // Otherwise, update the animation
    delay(FRAME_IN_MS);
    
    eyeptr++;
    if(eyeptr >=sizeof(eyeanim)) {
      // If we've hit the end, go back to the start and wait
      eyeptr=0;
      // Wait between 5-250 cycles before blinking again
      waittick = random(5,250);

      // 1 in 5 chance of him winking
      wink = random(1,5);
      if(wink != 1) {
        wink=0;
      }
    }

    // By default, have both eyes animate
    eyeptrL=eyeptrR=eyeptr;

    // If we're winking, keep the left eye open
    if(wink) {
      eyeptrL=0;
    }

  }
  
}

//
//  Draw the sprite on the display matrices.
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

void drawEyeL(unsigned char *ptr) {
  for( int row=0;row<8; row++) {
    sendData(LPANEL_TL,row+1,*ptr++);
    sendData(LPANEL_TR,row+1,*ptr++);
  }
  for( int row=0;row<8; row++) {
    sendData(LPANEL_BL,row+1,*ptr++);
    sendData(LPANEL_BR,row+1,*ptr++);
  }
}

//
//  Clear the display
//

void clearPanels() {
  for( int panel=0;panel<8; panel++) {
    for( int row=0;row<8; row++) {
      sendData(panel,row+1,0);
    }
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
