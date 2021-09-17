# syntheyes
Arduino code to display blinking eyes on MAX7219 driver boards

There is also an experimental raspberry Pi build, and the custom build for Arthi of Raptor's Den.

v3.14   - Add hook for internal LCD screen messages, change checkExpression API

v3.13   - Happy animation, fault animation with red status lights, fix status lights blip

v3.12   - Blushing animation

v3.11   - ACK LED and voice detection for status lights

v3.10   - Optionally drive neopixel status lights as well

v3.01   - Fix bug with procedural blinking

v3.00
	- Blinking is now done procedurally, so it is smoother and takes up less memory.
	- Sprites are now listed in binary, which makes it easier to see what's going on, but means the sprite editor won't work anymore.  I've included a hexadecimal version which the sprite editor can use.
	- Older versions (1.0 and 2.5.1) are now in a separate folder.
