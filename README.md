Hardware Mobile Adapter GB
==========================

This repository holds our hardware (and software) implementations of the Mobile Adapter GB device.

A generic library, called libmobile, is provided as a base for the different implementations.

Current implementations
-----------------------

- `arduino`: A generic arduino implementation, that should work on the Uno as well as other Arduino boards.
- `atmega328p`: An "arduino-less" implementation for the Atmega328p used by the Arduino Uno. It's a more bare-bones implementation meant to be used as reference for devices which do not have Arduino support.
- `bgb`: An implementation that communicates with the [BGB](http://bgb.bircd.org/) emulator over the bgblink protocol.

Refer to the implementation's README.md for more details.
