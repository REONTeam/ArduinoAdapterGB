Arduino Mobile Adapter GB
=========================

This is a hopefully "generic"-enough Arduino implementation of the Mobile Adapter GB.  
It has only been tested on the Arduino Uno board, for now, but there's a good chance it'll work on other AVR-based microcontrollers.


How to use
----------

Open `arduino.ino` in the Arduino IDE and upload it to your board. If you're using Windows, or checked out a zip from github, you might have to copy libmobile into the src directory.

The following pin configuration is required:
|  Pin | Arduino Uno |  Connection |
|------|-------------|-------------|
|  SCK |          13 |  Gameboy SC |
| MISO |          12 |  Gameboy SO |
| MOSI |          11 |  Gameboy SI |
|   SS |          10 |         GND |
|  GND |         GND | Gameboy GND |

NOTE: If this doesn't work, try to flip around SO and SI, as the pinout markings of your link cable breakout might be the other way around.
