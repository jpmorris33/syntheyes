//
//  Synth Eyes for Arduino
//  Unicorn Hat HD example
//
//  Based on example code from  https://gist.github.com/nrdobie/8193350  among other sources
//
//  Copyright (c) 2021 J. P. Morris
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

#define STATUS_LIGHTS     // Drive neopixel status lights in the horns
#define VOICE_DETECTOR    // Flash the status lights if a microphone detects something

#include <SPI.h>
#ifdef STATUS_LIGHTS
#include <FastLED.h>
#endif

// Configurables, adjust to taste

// RGB triplets for the eye colour, default to yellow (full red, half green, no blue)
#define EYE_RED   0xff
#define EYE_GREEN 0x80
#define EYE_BLUE  0x00

// RGB triplets for the status light colour, default to yellow (full red, half green, no blue)
#define COLOUR_RED   0xff
#define COLOUR_GREEN 0x80
#define COLOUR_BLUE  0x00

#define BRIGHTNESS  12  // Brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)
#define STATUSBRIGHT 100
#define VOICEBRIGHT 255
#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds (20 default)
#define WAIT_IN_MS  60  // Delay per tick in milliseconds when waiting to blink again (60 default)
#define MIN_DELAY    5   // Minimum delay between blinks
#define MAX_DELAY    250 // Maximum delay between blinks
#define STATUS_DIVIDER 32  // This controls the speed of the status light chaser, bigger is slower (def 32)

#define CS_LEFT_PIN 10       // Chip select pin for left eye panel
#define CS_RIGHT_PIN 9       // Chip select pin for right eye panel
#define STATUS_PIN 5    // Neopixels DIN pin for status LEDs
#define ACK_LED_PIN A0  // Flashes briefly when receiving an expression input to signal that you've got it
#define VOICE_PIN A1    // Audio input for flashing the status pins
#define ADC_THRESHOLD 128 // Voice activation threshold

#ifndef OVERRIDE_PINS
  #define OWO_PIN 7
  #define ANNOYED_PIN 6
  #define FAULT_PIN 8
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

#define PIXELS 128
#define PIXELS_PER_PANEL 64
#define STATUSPIXELS 6

#define STEPS (STATUSPIXELS*2)

// Functions

void drawEyeL();
void drawEyeR();
void getSprite(unsigned char *ptr, int blinkpos);
void sendData(int addr, byte opcode, byte data);
void sendEye(unsigned char *ptr);
void wait(int ms, bool interruptable);
void getNextAnim();
bool checkExpression(struct STATES *state);
void statusCycle(unsigned char r, unsigned char g, unsigned char b);
void setMessage(char *msg);

// System state variables

#define WAITING -1
#define BLINK 0
#define WINK 1
#define ROLLEYE 2
#define OWO 3
#define ANNOYED 4
#define BLUSH 5
#define HAPPY 6
#define FAULT 7

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
unsigned char spidata[769];

// Frame buffer for procedural blink
unsigned char framebuffer[16][2];

// Status light variables
#ifdef STATUS_LIGHTS
CRGB statusbuffer[STATUSPIXELS];
CRGB colour(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
CLEDController *statusController;
unsigned char ramp[STEPS];
#endif

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

char owo[] = {0, 2, 3, 4, -30, 4, 3, 2, 0, -10};

signed char annoyed[] = {0, 5, 6, -30, 6, 5, 0, -10};

signed char fault[] = {0,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4,7,-4,1,-4};

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
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000011, B11000000,
      B00001111, B11111000,
      B00001111, B11111100,
      B00101111, B11111110,

      B01101111, B11111111,
      B01101111, B11111111,
      B11101111, B11111111,
      B11101111, B11111111,
      B01111111, B11111110,
      B00000111, B11100000,
      B00000000, B00000000,
      B00000000, B00000000,
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
    // OwO eye 1 (2)
    {
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000011, B11000000,
      B00000111, B11111000,
      B00000111, B11111100,
      B00110111, B11111110,
      B01110111, B11111110,
      B01110111, B11111110,
      B01110111, B11111110,
      B01111111, B11111110,
      B00111111, B11111100,
      B00001111, B11110000,
      B00000111, B11100000,
      B00000000, B00000000,
      B00000000, B00000000,
    },
    // OwO eye 2 (3)
    {
      B00000000, B00000000,
      B00000000, B00000000,
      B00000011, B11000000,
      B00000011, B11111000,
      B00010011, B11111100,
      B00110011, B11111100,
      B01110011, B11111110,
      B01110011, B11111110,
      
      B01110011, B11111110,
      B01111111, B11111110,
      B01111111, B11111110,
      B00111111, B11111100,
      B00011111, B11110000,
      B00000111, B11100000,
      B00000000, B00000000,
      B00000000, B00000000,
    },
    // OwO eye 3 (4)
    {
      B00000000,B00000000,
      B00000001,B11000000,
      B00001001,B11110000,
      B00011001,B11111000,
      B00111001,B11111100,
      B00111001,B11111100,
      B01111001,B11111110,
      B01111101,B11111110,
      B01111111,B11111110,
      B01111111,B11111110,
      B00111111,B11111100,
      B00111111,B11111100,
      B00011111,B11111000,
      B00001111,B11110000,
      B00000011,B11000000,
      B00000000,B00000000,
    },
    // annoyed 1 (5)
    {
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000000, B00000000,
      B00000111, B11111100,
      B00101111, B11111111,
      B01101111, B11111111,
      B01101111, B11111111,
      B11101111, B11111111,
      B01111111, B11111110,
      B00011111, B11111000,
      B00000111, B11100000,
      B00000000, B00000000,
      B00000000, B00000000,
    },
    // annoyed 2 (6)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B01110111,B11111111,
      B11110111,B11111111,
      B11110111,B11111110,
      B01111111,B11111100,
      B00111111,B11111000,
      B00011111,B11110000,
      B00000111,B11000000,
      B00000000,B00000000,
      B00000000,B00000000,
    },
    // fault (7)
    {
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00011000,B00011000,
      B00001100,B00110000,
      B00000110,B01100000,
      B00000011,B11000000,
      B00000001,B10000000,
      B00000011,B11000000,
      B00000110,B01100000,
      B00001100,B00110000,
      B00011000,B00011000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
      B00000000,B00000000,
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
  char *description;
};

// Add any new animation triggers here

struct STATES states[] = {
{BLINK,     closeeye,    sizeof(closeeye), 0,             "Blinking"},
{WINK,      closeeye,    sizeof(closeeye), 0,             "Winking"},
{ANNOYED,   annoyed,     sizeof(annoyed),  ANNOYED_PIN,   "Annoyed"},
{OWO,       owo,         sizeof(owo),      OWO_PIN,       "OWO"},
{FAULT,     fault,       sizeof(fault),    FAULT_PIN,     "Fault!"},
// DO NOT REMOVE THIS LAST LINE!
{0,         NULL,        0,                0}  
};


//
//  Here's the actual implementation
//

void setup() {
  pinMode(CS_LEFT_PIN,OUTPUT);
  pinMode(CS_RIGHT_PIN,OUTPUT);
  digitalWrite(CS_LEFT_PIN, LOW);
  digitalWrite(CS_RIGHT_PIN, LOW);
  pinMode(ACK_LED_PIN,OUTPUT);
  digitalWrite(ACK_LED_PIN, LOW);

  // Init pins for states
  for(int ctr=0;states[ctr].anim;ctr++) {
    if(states[ctr].pin) {
      pinMode(states[ctr].pin, INPUT_PULLUP);
    }
  }
  
  // Set up data transfers
  SPI.begin();
  SPI.beginTransaction( SPISettings(9000000, MSBFIRST, SPI_MODE0 ) );
  digitalWrite(CS_LEFT_PIN, HIGH);
  digitalWrite(CS_RIGHT_PIN, HIGH);

  setMessage("");

#ifdef STATUS_LIGHTS
  // Initialise the status lights
  statusController = &FastLED.addLeds<NEOPIXEL, STATUS_PIN>(statusbuffer,STATUSPIXELS);
  for(int ctr=0;ctr<STATUSPIXELS;ctr++) {
    statusbuffer[ctr]=CRGB::Red;
  }
  statusController->showLeds(STATUSBRIGHT);

  // Build lookup table for pulsating status lights
  // I'm sure there's a smarter way to do this on the fly, but...
  int ramping=0;
  for(int ctr=0;ctr<STEPS;ctr++) {
    ramp[ctr]=ramping;
    if(ctr<STEPS/2) {
      ramping+=(256/(STEPS*2));
    } else {
      ramping-=(256/(STEPS*2));
    }
  }
#endif

  randomSeed(0);  

  eyeanim = &closeeye[0];
  eyemax = sizeof(closeeye);
  state = BLINK;

  // Initial draw
  getSprite(&eye[frameidx][0], 0);
  drawEyeR();
  drawEyeL();
  statusCycle(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
}

void loop() {
    // Draw the sprites
    getSprite(&eye[frameidx][0], blinkidx);
    drawEyeR();
    getSprite(&eye[frameidx][0], state == WINK ? 0 : blinkidx);
    drawEyeL();

   digitalWrite(ACK_LED_PIN, LOW);

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

  if(state > WAITING) {
    setMessage(states[state].description);
  } else {
    setMessage("");
  }

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

  unsigned char *out=spidata;
  unsigned int pix=0;
  int x,y;
  memset(spidata,0,769);
  *out++ = 0x72; // Header

  for(y=0;y<16;y++) {
    // Grab the entire 16 pixel row
    pix=*ptr++;
    pix<<=8;
    pix|=*ptr++;

    for(x=0;x<16;x++) {
        if(pix & 0x0001) {
            *out++ = EYE_RED;
            *out++ = EYE_GREEN;
            *out++ = EYE_BLUE;
        } else {
            *out++ = 0;
            *out++ = 0;
            *out++ = 0;
        }
        pix >>=1;
    }
  }

  digitalWrite(CS_RIGHT_PIN,LOW);
  SPI.transfer(spidata,769);
  digitalWrite(CS_RIGHT_PIN,HIGH);

  // Don't send again until the frame changes
  updateR=false;
  
}

//
//  Draw the mirrored sprite on the 'left' matrix
//

void drawEyeL() {
  unsigned char *ptr = &framebuffer[0][0];

  if(!updateL) {
    return;
  }

  unsigned char *out=spidata;
  unsigned int pix=0;
  int x,y;
  memset(spidata,0,769);
  *out++ = 0x72; // Header

  for(y=0;y<16;y++) {
    // Grab the entire 16 pixel row
    pix=*ptr++;
    pix<<=8;
    pix|=*ptr++;

    for(x=0;x<16;x++) {
        if(pix & 0x8000) {
            *out++ = EYE_RED;
            *out++ = EYE_GREEN;
            *out++ = EYE_BLUE;
        } else {
            *out++ = 0;
            *out++ = 0;
            *out++ = 0;
        }
        pix <<=1;
    }
  }

  digitalWrite(CS_LEFT_PIN,LOW);
  SPI.transfer(spidata,769);
  digitalWrite(CS_LEFT_PIN,HIGH);

  // Don't send again until the frame changes
  updateL=false;
}

//
//  Wait, and cue up a reaction if we detect one via GPIO
//

void wait(int ms, bool interruptable) {
  for(int ctr=0;ctr<ms;ctr++) {
    delay(1);
    statusCycle(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
    if(state == WAITING) {
      for(int ctr2=0;states[ctr2].anim;ctr2++) {
        if(states[ctr2].pin) {
          if(checkExpression(&states[ctr2])) {
            nextstate = states[ctr2].id;
            digitalWrite(ACK_LED_PIN, HIGH);
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

static int maxpos = 255 - (255%STEPS);

void statusCycle(unsigned char r, unsigned char g, unsigned char b) {
#ifdef STATUS_LIGHTS
  uint16_t ctr;
  static uint16_t pos=0;
  static int divider=0;
  int maxdiv=STATUS_DIVIDER;
  #ifdef VOICE_DETECTOR
    bool bright = !(analogRead(VOICE_PIN) > ADC_THRESHOLD);
  #else
    bool bright = false;
  #endif

  divider++;
  if(divider > maxdiv) {
    pos++;
    divider=0;
  }
  if(pos > maxpos) {
    pos=0;
  }

  for(ctr=0; ctr< STATUSPIXELS; ctr++) {
    // If they're in an emergency state, make the lights red
    if(state == FAULT) {
      statusbuffer[ctr] = CRGB(255,0,0);
    } else {
      statusbuffer[ctr] = CRGB(r,g,b);
    }
    if(bright) {
      statusbuffer[ctr].nscale8(255);
    } else {
      if(state == FAULT) {
        // All blink together
        statusbuffer[ctr].nscale8(ramp[pos%STEPS]);
      }
      else {
        // Fairground effect
        statusbuffer[ctr].nscale8(ramp[(ctr+pos)%STEPS]);
      }
    }
  }

  statusController->showLeds(bright?VOICEBRIGHT:STATUSBRIGHT);
#endif    
}


//
//  Default handler for Arduino, can be replaced for other platfirms
//

#ifndef CUSTOM_EXPRESSION_HANDLER
bool checkExpression(STATES *state) {
    return (digitalRead(state->pin) == LOW);
}
#endif

//
//  Display state on an LCD display (exercise for the reader)
//

void setMessage(char *msg) {
}
