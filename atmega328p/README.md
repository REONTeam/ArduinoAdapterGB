Atmega328p Mobile Adapter GB
============================

This is an "arduino-less", more lightweight Atmega328p implementation of the Mobile Adapter GB.  
This is useful as a reference implementation for those chips that don't have an Arduino implementation, and hopefully isn't too hard to port to similar AVR chips.


How to use
----------

Install `make`, `avr-gcc`, `avr-libc` and `avrdude` for your distribution, for example, on debian: `sudo apt install make gcc-avr avr-libc avrdude`  
If `avr-gcc` isn't available on your distribution, you might want to add the `hardware/tools/avr/bin` directory of the Arduino IDE to your `PATH`.  
Alternatively, you can install it from source through [crosstool-ng](https://github.com/crosstool-ng/crosstool-ng) (use the `avr` sample config), or [the arduino build scripts](https://github.com/arduino/toolchain-avr).

Once the requirements have been satisfied, run `make`. To upload it to the board, run `make upload`, or `make SERIAL=/dev/ttyACM0 upload`, changing `ttyACM0` to whatever port your arduino is connected to.

The following pin configuration is required:

|      Pin |  Connection |
| -------- | ----------- |
| 19 (PB5) |  Gameboy SC |
| 18 (PB4) |  Gameboy SO |
| 17 (PB3) |  Gameboy SI |
| 16 (PB2) |         GND |
|  8 (GND) | Gameboy GND |

NOTE: If this doesn't work, try to flip around SO and SI, as the pinout markings of your link cable breakout might be the other way around.
