extern void eyesSetup();
extern void setup();
extern void loop();

int main(int argc, char *argv[]) {
setup();
eyesSetup();
for(;;) loop();
}
