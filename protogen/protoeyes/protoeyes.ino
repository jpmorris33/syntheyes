//
//  Protogen Eyes for Arduino
//  V2.0.3 - Improve stability of Defender mode
//  V2.0.2 - Defender mode, Use Nano LED for user feedback on death/psycho modes
//  V2.0.1 - Death Mode
//  V2.0.0 - Single board
//
//  Based on example code from  https://gist.github.com/nrdobie/8193350  among other sources
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

//
//  Wire the driver board to the arduino with the following pinout:
//
//  CS (left eye) - pin 9 (this can be changed, see below)
//  CS (right eye) - pin 10 (this can be changed, see below)
//  DIN - pin 11 (this is fixed by the SPI library) - send this output to both eyes
//  CLK - pin 13 (this is fixed by the SPI library) - send this output to both eyes
//
//  My panels were arranged with the drivers cascaded eye1->eye2->mouth1->mouth2->mouth3->mouth4->nostril
//  however the image is drawn upside-down.
//  If your panels are in a different order, adjust the constants below
//


#include <SPI.h>

//#define ENABLE_PSYCHO    // Warning, mad protogen
#define ENABLE_DEFENDER  // Shall we play a game?
//#define DEAD_PROTO     // This could be triggered by a GPIO pin but I made it for a photoshoot

// Configurables, adjust to taste

#define BRIGHTNESS  2  // Brightness from 0-15.  You may need to adjust this   (TW: was 2, set to 12 for use with red filter)
#define FRAME_IN_MS 20  // Delay per animation frame in milliseconds (20 default)
#define WAIT_IN_MS  60  // Delay per tick in milliseconds when waiting to blink again (60 default)
#define MIN_DELAY    5   // Minimum delay between blinks
#define MAX_DELAY    250 // Maximum delay between blinks

#define CS_RIGHT 10       // Chip select pins
#define CS_LEFT 9       // Chip select pins

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

#define EYE_1    1
#define EYE_2    0
#define MOUTH_1  5
#define MOUTH_2  4
#define MOUTH_3  3
#define MOUTH_4  2
#define NOSTRIL  6
#define BLANK  7

// System constants, you probably don't want to touch these

#define CMD_TEST      0x0f
#define CMD_INTENSITY 0x0a
#define CMD_SCANLIMIT 0x0b
#define CMD_DECODE    0x09
#define CMD_SHUTDOWN  0x0C

// Functions

void initPanels(int brightness);
void drawEyeL();
void drawEyeR();
void getSprite(unsigned char *ptr, int blinkpos);
void sendData(int cs, int addr, byte opcode, byte data);
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
bool updateL=true;
bool updateR=true;

// transfer buffer for programming the display chip
unsigned char spidata[16];

// Frame buffer for procedural blink
unsigned char framebuffer[8][2];

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
// These are monochrome sprites, 0 is dark, 1 is lit
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
    // BOOT MASTER (5) // This is a throwback to the old 2-processor version saying which one was in charge but I've left it in for now
    {
      B11111111,B00000000,
      B10000001,B00000000,
      B10111101,B00000000,
      B10111101,B00000000,
      B10111101,B00000000,
      B10111101,B00000000,
      B10000001,B00000000,
      B11111111,B00000000,
    },    
    // BOOT FOLLOWER (6)
    {
      B11111111,B00000000,
      B10000001,B00000000,
      B10000001,B00000000,
      B10000001,B00000000,
      B10000001,B00000000,
      B10000001,B00000000,
      B10000001,B00000000,
      B11111111,B00000000,
    },    
    // Psycho mode (7)
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
    // Psycho mode flipped (8)
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
  pinMode(CS_LEFT,OUTPUT);
  pinMode(CS_RIGHT,OUTPUT);
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
  getSprite(eye[5], 0);
  drawEyeR();
  getSprite(eye[6], 0);
  drawEyeL();

  delay(2000);

  // Initial draw
  getSprite(&eye[0][0], 0);
  drawEyeR();
  drawEyeL();

  // HACK
#ifdef DEAD_PROTO  
  state = DEAD;
  nextstate = DEAD;
  getNextAnim();
#endif
  
}

void initPanels(int brightness) {
  // Initialise the panels
  for(int panel=0;panel<8;panel++) {
      sendData(CS_LEFT, panel, CMD_TEST,0);
      sendData(CS_LEFT, panel, CMD_DECODE,0);    // Disable BCD decoder so we can sent sprite data instead
      sendData(CS_LEFT, panel, CMD_INTENSITY,brightness);
      sendData(CS_LEFT, panel, CMD_SCANLIMIT,7);
      sendData(CS_LEFT, panel, CMD_SHUTDOWN,1);  // 0 turns it off, 1 turns it on
  }

  // Initialise the panels
  for(int panel=0;panel<8;panel++) {
      sendData(CS_RIGHT, panel, CMD_TEST,0);
      sendData(CS_RIGHT, panel, CMD_DECODE,0);    // Disable BCD decoder so we can sent sprite data instead
      sendData(CS_RIGHT, panel, CMD_INTENSITY,brightness);
      sendData(CS_RIGHT, panel, CMD_SCANLIMIT,7);
      sendData(CS_RIGHT, panel, CMD_SHUTDOWN,1);  // 0 turns it off, 1 turns it on
  }
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
      getSprite(&eye[0][0], 8);
      update_defender();
      updateL=updateR=true;
      initPanels(BRIGHTNESS); // Reboot the panels every frame, this is a horrible workaround (NB: we could use this to modulate the brightness by voice)
      drawEyeL();
      drawEyeR();
      return;
    }
#endif
  
   // Draw the sprites
   getSprite(&eye[frameidxL][0], state == WINK ? 0 : blinkidx);
   drawEyeL();
   getSprite(&eye[frameidxR][0], blinkidx);
   drawEyeR();

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
    updateL=updateR=true;
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
    
    updateL=updateR=true;
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

void getSprite(unsigned char *ptr, int blinkpos) {
  memcpy(framebuffer,ptr,16);
  if(blinkpos > 0) {
    if(blinkpos > 8) {
      blinkpos = 8;
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

  for( int row=7;row>=0; row--) {
    sendData(CS_RIGHT,EYE_1,row+1,*ptr++);
    sendData(CS_RIGHT,EYE_2,row+1,*ptr++);
  }

  ptr = &mouth[mouthptr][0];
  for( int row=7;row>=0; row--) {
    sendData(CS_RIGHT,MOUTH_1,row+1,*ptr++);
    sendData(CS_RIGHT,MOUTH_2,row+1,*ptr++);
    sendData(CS_RIGHT,MOUTH_3,row+1,*ptr++);
    sendData(CS_RIGHT,MOUTH_4,row+1,*ptr++);
  }

  // Nose is upside-down
  ptr = &nostril[0];
  for( int row=7;row>=0; row--) {
    sendData(CS_RIGHT,NOSTRIL,row+1,*ptr++);
  }

  // Don't send again until the frame changes
  updateR=false;
  
}

//
//  Draw the mirrored sprite on the 'left' matrix
//  Thanks to the cable routing the panels are upside-down so we only need to flip vertically
//

void drawEyeL() {
  unsigned char *ptr = &framebuffer[0][0];

  if(!updateL) {
    return;
  }

  for( int row=0;row<8; row++) {
    sendData(CS_LEFT,EYE_1,row+1,*ptr++);
    sendData(CS_LEFT,EYE_2,row+1,*ptr++);
  }

  ptr = &mouth[mouthptr][0];
  for( int row=0;row<8; row++) {
    sendData(CS_LEFT,MOUTH_1,row+1,*ptr++);
    sendData(CS_LEFT,MOUTH_2,row+1,*ptr++);
    sendData(CS_LEFT,MOUTH_3,row+1,*ptr++);
    sendData(CS_LEFT,MOUTH_4,row+1,*ptr++);
  }

  ptr = &nostril[0];
  for( int row=0;row<8; row++) {
    sendData(CS_LEFT,NOSTRIL,row+1,*ptr++);
  }

  // Don't send again until the frame changes
  updateL=false;
  
}


//
//  Send a command or data to the display chip
//

void sendData(int cs, int addr, byte opcode, byte data) {
  int offset=addr*2;
  memset(spidata,0,16);
  
  // Data is put into the array in reverse order
  // because the chip wants it sent that way
  spidata[15-(offset+1)]=opcode;
  spidata[15-offset]=data;

  // Blit it to the display
  digitalWrite(cs,LOW);
  SPI.transfer(spidata,16);
  digitalWrite(cs,HIGH);
}

//
//  Wait, and cue up a reaction if we detect one via GPIO
//

void wait(int ms, bool interruptable) {
  for(int ctr=0;ctr<ms;ctr++) {
    delay(1);
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
