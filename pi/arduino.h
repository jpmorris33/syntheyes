typedef unsigned char byte;
#define PROGMEM
#define pgm_read_byte(a) *(a)
#define SPISettings(a,b,c) 0
#define INPUT_PULLUP 0

class SPIlib {
	public:
	SPIlib();
	void begin();
	void beginTransaction(int nothing);
	void transfer(unsigned char *ptr, int len);
};

extern void randomSeed(unsigned int r);
extern int random(int lowest, int highest);
