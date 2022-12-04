#include <FastLED.h>

//
//  Synth Eyes for Arduino Neopixels
//  V1.1.1 - Allow expressions to be different colours
//
//  Copyright (c) 2022 J. P. Morris
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


// Configurables, adjust to taste

//#define COLOUR_WHEEL  // Cycle through colours instead of having a static colour

// RGB triplets for the eye colour, default to yellow (full red, full green, no blue)
#define COLOUR_RED   0xff
#define COLOUR_GREEN 0xff
#define COLOUR_BLUE  0x00

#define BRIGHTNESS  4  // Brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)
#define STATUSBRIGHT 100
#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds (20 default)
#define WAIT_IN_MS  60  // Delay per tick in milliseconds when waiting to blink again (60 default)
#define MIN_DELAY    5   // Minimum delay between blinks
#define MAX_DELAY    250 // Maximum delay between blinks
#define STATUS_DIVIDER 32  // This controls the speed of the status light chaser, bigger is slower

#define EYE_PIN 6
#define STATUS_PIN 5

#ifndef OVERRIDE_PINS
	#define STARTLED_PIN 7
	#define EYEROLL_PIN 8
	#define ANNOYED_PIN 9
#endif


#ifdef COLOUR_WHEEL
  // The colour wheel polling means we need different timings for the delays
  #undef FRAME_IN_MS
  #define FRAME_IN_MS 3
  #undef WAIT_IN_MS
  #define WAIT_IN_MS  6
#endif

//
//  Panel positions for each corner of the sprite
//
//  If the sprite appears garbled, with the wrong displays showing each corner
//  because your panels are wired differently to mine, you may need to adjust
//  the ID of each panel here, e.g. 0-3 instead of 3-0
//

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
void wait(int ms, bool interruptable);
void getNextAnim();
bool checkExpression(int pin);
void statusCycle(unsigned char r, unsigned char g, unsigned char b);

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

CRGB framebuffer[PIXELS];
CRGB statusbuffer[STATUSPIXELS];
unsigned char bitfb[8];
CRGB colour(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
CRGB colour_default(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
CRGB colour_red(CRGB::Red);
CLEDController *eyeController;
CLEDController *statusController;

unsigned char ramp[STEPS];

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
// These are 8x8 monochrome sprites, 0 is dark, 1 is lit
//
// You could also add extra animation frames and logic, e.g. to roll the eyes
// Or to make them animate a pixel at a time instead of two pixels
//
// Note that these are currently stored in dynamic memory so there's a 2KB limit,
// Putting the sprites into program memory will give you about 30KB to play with
// but you'd have to modify the software to read them from that address space.
//

// Right eye (facing left)
unsigned char eye[][8] = {
    // Basic open eye (0)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // closed (blank) (1)
    {
      B00000000,
      B00000000,
      B00000000,
      B00000000,
      B00000000,
      B00000000,
      B00000000,
      B00000000,
    },
    // roll eye 1 (2)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // roll eye 2 (3)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // roll eye 3 (4)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // roll eye 4 (5)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // roll eye 5 (6)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // roll eye 6 (7)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
    // startled 1 (8)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01111111,
      B01011111,
      B11011111,
      B11111110,
    },
    // startled 2 (9)
    {
      B00000000,
      B00000000,
      B00000000,
      B00011110,
      B01111111,
      B01011111,
      B11111111,
      B11111110,
    },
  // Annoyed 1 (10)
    {
      B00000000,
      B00000000,
      B00000000,
      B00001110,
      B01011111,
      B01011111,
      B11011111,
      B11111110,
    },
// Annoyed 2 (11)
    {      
      B00000000,
      B00000000,
      B00000000,
      B00000000,
      B00000011,
      B01011111,
      B11011111,
      B11111110,
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
  CRGB *colour;
};

// Add any new animation triggers here

struct STATES states[] = {
{BLINK,     closeeye,    sizeof(closeeye), 0,             NULL},
{WINK,      closeeye,    sizeof(closeeye), 0,             NULL},
{ROLLEYE,   rolleye,     sizeof(rolleye),  EYEROLL_PIN,   NULL},
{STARTLED,  startled,    sizeof(startled), STARTLED_PIN,  NULL, },
{ANNOYED,   annoyed,     sizeof(annoyed),  ANNOYED_PIN,   &colour_red},
// DO NOT REMOVE THIS LAST LINE!
{0,         NULL,        0,                0,             NULL}  
};

//
//  Here's the actual implementation
//

void setup() {
  // Init pins for states
  for(int ctr=0;states[ctr].anim;ctr++) {
    if(states[ctr].pin) {
      pinMode(states[ctr].pin, INPUT_PULLUP);
    }
  }
  
  // Clear the framebuffer

  eyeController = &FastLED.addLeds<NEOPIXEL, EYE_PIN>(framebuffer,PIXELS);
  
  for(int ctr=0;ctr<PIXELS;ctr++) {
    framebuffer[ctr]=CRGB::Black;
  }
  eyeController->showLeds(BRIGHTNESS*2);

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
  
  
  randomSeed(0);  

  eyeanim = &closeeye[0];
  eyemax = sizeof(closeeye);
  state = BLINK;

  // Initial draw
  getSprite(&eye[frameidx][0], 0);
  drawEyeR();
  drawEyeL();
}

#ifdef COLOUR_WHEEL

void updateWheel() {

  static unsigned char wheelR=60;
  static unsigned char wheelG=30;
  static unsigned char wheelB=10;
  static char wheelDirR=2;
  static char wheelDirG=-1;
  static char wheelDirB=1;
  
  colour.red = wheelR;
  colour.green = wheelG;
  colour.blue = wheelB;

  wheelR += wheelDirR;
  if(wheelR >= 254 || wheelR < 2) {
    wheelDirR=-wheelDirR;
  }
  wheelG += wheelDirG;
  if(wheelG >= 254 || wheelG < 2) {
    wheelDirG=-wheelDirG;
  }
  if(wheelB >= 254 || wheelB < 2) {
    wheelDirB=-wheelDirB;
  }
  
  updateL=updateR=true;

    getSprite(&eye[frameidx][0], blinkidx);
    drawEyeR();
    getSprite(&eye[frameidx][0], state == WINK ? 0 : blinkidx);
    drawEyeL();    

    statusCycle(wheelR,wheelG,wheelB);
    
}
#endif

void loop() {

#ifdef COLOUR_WHEEL
  updateWheel();
#else
    // Draw the sprites
    getSprite(&eye[frameidx][0], blinkidx);
    drawEyeR();
    getSprite(&eye[frameidx][0], state == WINK ? 0 : blinkidx);
    drawEyeL();
    statusCycle(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
#endif

  
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
      colour = colour_default;
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

  colour = colour_default;

  for(ctr=0;states[ctr].anim;ctr++) {
    if(states[ctr].id == nextstate) {
      eyeanim = states[ctr].anim;
      eyemax = states[ctr].animlen;
      if(states[ctr].colour) {
        colour = *states[ctr].colour;
      }
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
  memcpy(bitfb,ptr,8);
  if(blinkpos > 0) {
    if(blinkpos > 8) {
      blinkpos = 8;
    }
    memset(bitfb,0,blinkpos);
  }
}


//
//  Draw the sprite on the 'right' display matrix
//

void drawEyeR() {

  CRGB *fbptr = &framebuffer[0];
  unsigned char *ptr = &bitfb[0];
  unsigned char bits;
  
  if(!updateR) {
    return;
  }

  // Then whatever's left from the sprite pointer
  for(int ctr=0;ctr<8;ctr++) {
    bits = *ptr++;
    for(int col=0;col<8;col++) {
      if(bits & 0x80) {
        *fbptr++ = colour;
      } else {
        *fbptr++ = CRGB::Black;
      }
      bits <<= 1;
    }
  }

  eyeController->showLeds(BRIGHTNESS*2);

  // Don't send again until the frame changes
  updateR=false;
  
}

//
//  Draw the mirrored sprite on the 'left' matrix (note the L&R panels are swapped)
//

void drawEyeL() {
  CRGB *fbptr = &framebuffer[PIXELS_PER_PANEL];
  unsigned char *ptr = &bitfb[0];
  unsigned char bits;
  
  if(!updateL) {
    return;
  }

  // Then whatever's left from the sprite pointer
  for(int ctr=0;ctr<8;ctr++) {
    bits = *ptr++;
    for(int col=0;col<8;col++) {
      if(bits & 0x01) {
        *fbptr++ = colour;
      } else {
        *fbptr++ = CRGB::Black;
      }
      bits >>= 1;
    }
  }

  eyeController->showLeds(BRIGHTNESS*2);

  // Don't send again until the frame changes
  updateL=false;
  
}

//
//  Wait, and cue up a reaction if we detect one via GPIO
//

void wait(int ms, bool interruptable) {
  for(int ctr=0;ctr<ms;ctr++) {
    delay(1);

#ifdef COLOUR_WHEEL
    updateWheel();
#else
    statusCycle(COLOUR_RED,COLOUR_GREEN,COLOUR_BLUE);
#endif
    
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

void statusCycle(unsigned char r, unsigned char g, unsigned char b) {
  uint16_t ctr;
  static uint16_t pos=0;
  static int divider=0;
  int maxdiv=STATUS_DIVIDER;
  #ifdef COLOUR_WHEEL
    maxdiv /= 5;
  #endif

  divider++;
  if(divider > maxdiv) {
    pos++;
    divider=0;
  }
  if(pos > 255) {
    pos=0;
  }

  for(ctr=0; ctr< STATUSPIXELS; ctr++) {
    statusbuffer[ctr] = CRGB(r,g,b);
    statusbuffer[ctr].nscale8(ramp[(ctr+pos)%STEPS]);
  }
    statusController->showLeds(STATUSBRIGHT);
}


//
//  Default handler for Arduino, can be replaced for other platfirms
//

#ifndef CUSTOM_EXPRESSION_HANDLER
bool checkExpression(int pin) {
    return (digitalRead(pin) == LOW);
}
#endif
