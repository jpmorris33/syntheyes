typedef unsigned char byte;
#define PROGMEM
#define pgm_read_byte(a) *(a)
#define SPISettings(a,b,c) a
#define INPUT_PULLUP INPUT

class SPIlib {
	public:
	SPIlib();
	void begin();
	void beginTransaction(long speed);
	void transfer(unsigned char *ptr, int len);
};

extern void randomSeed(unsigned int r);
extern int random(int lowest, int highest);
