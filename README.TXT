														UNO2IEC README


DISCLAIMER:
The author is in no way responsible for any problems or damage caused by
using this code. Use at your own risk.

LICENSE:
This code is distributed under the GNU Public License
which can be found at http://www.gnu.org/licenses/gpl.txt



================================================================================

Description
------------
The UNO2IEC device simulates a 1541 drive and enables loading and saving files or
programs between a CBM and any "current" folder on the host system. The host system location can be network share,
USB, SD flash media, SSD or any hard disk that is supported by a Windows, Linux or Mac operating system.

The direct commununication with the CBM is an arduino (uno or similar) communicating over a regular serial line
with the media host system. The serial line can be either the gpio pin mapped serial port on the arduino or the
serial over USB port. In case using the USB-over-serial port, the benefit is automatic reset of the arduino when the
host connects to it.

Note: As of writing the project does not yet support any turboloaders. There are however plans or ideas to provide
this support. It should be possible with some extra work of performance tuning optimum handling of the serial interface.

The project inherits both code and ideas from the MMC2IEC project, originally written by
Jan Derogee's and Lars Pontoppidan. Their original work is great and highly respected.
The code in this project however, has been subject to heavy redesign. While ported to C++ the idea is to let the
old commodore targeted media be transferred from a PC host rather than just a MMC or SD card.
The design of this project required the IEC protocol between the CBM and the emulated 1541 to be refactored into having
splitted responsibilities between the arduino and the PC host.
A secondary RS232-serial based protocol between the arduino is utilized to be able to transfer the actual media to
the CBM.


Target device
-------------
Atmel ATMega328 (Arduino Uno) at ~16 MHz, 32KB flash, 2 KB SRAM, 1KB EEPROM.


Contact
-------
This project was created by:
		Lars Wadefalk, email: lars@wadefalk.se

Project homepage:
		http://TODO.com


Hardware setup
--------------
Arduino Uno: ATmega 16U2 328 microcontroller @ 16Mhz, 32KB flash, SRAM 2KB, EEPOM 1 KB.
Connect at least ATN, CLOCK, DATA and GND pin to the IEC bus on the CBM. The Reset and SRQIN (latter one is unused on the C64)
are optional to wire.
The Uno can be bought very cheap at dx.com:
http://dx.com/p/uno-r3-development-board-microcontroller-mega328p-atmega16u2-compat-for-arduino-blue-black-215600


Optional: LED matrix with MAX7219 controller.
Can be bought cheap at dx.com with 5 dupont lines:
http://dx.com/p/max7219-dot-matrix-module-w-5-dupont-lines-184854
Whether or not using the LED matrix when compiling, either define or comment out the USE_LED_DISPLAY macro in the
global_defines.h header in the uno2iec project.

Either connect Arduino to a PC, MAC. Media host side project can be compiled on either Linux, Windows or Mac desktop.
It works on the Raspberry Pi as a host as well. Connect two serial pins (RX, TX, twisted of course) on the PI to the arduino. A third
pin may also be connected with the purpose to reset the Arduino from the PI side (the PI uses the wiringPi project for GPIO access). The 
raspbian image is the suggested linux to use, it needs Qt build and runtime support.
For more info about how to prepare a raspberry pi properly for external serial communication, read the notes.txt file. There are a few 
details when it comes to releasing the serial port from the kernel log.


How to build
------------
Open the PC/Raspberry project (.pro) file in Qt creator and build either release or debug.

REQUIRES: (external dependencies):
QExtSerialPort project:
https://code.google.com/p/qextserialport/

The QExterSerialPort project should be located on the same directory level as the rpi2iec project.


Optional: Max7219 driver library (modified by Lars Wadefalk to c++ with support for progress bar display and scroller).
Github project:

First build the Qt project on the desired platform. Then build and deploy the arduino project using the latest arduino
suite to the arduino target.


Files in release:
-----------------
README.TXT (this file)
notes.txt
TODO: changes.txt
TODO: license.txt

./		Under repo root is the PC/Raspberry Qt project files.
uni2iec/	All files under here is the arduino sketch.

What else?
----------
Ideas and improvements are highly welcome. Please contribute to this project!
The retro machine community gains from it.

There are a few coding guidelines I would like to be followed (except for the ones who are obvious by just looking
at the code).

The c++ files should have cpp and hpp file extensions.
Tabs instead of spaces are used (!), personally I use 2 indents (blanks) for each TAB.
Using TABs gives us more freedom.

There is no limitation to 80 columns at all, but use common sense.
For splitted lines, operators are placed at the beginning of each new line to make code more clear (Qt style).

Try to use constants rather than preprocess defines, and try to keep methods/functions const, if possible.

Use camelCase.

Blanks between operators, NOT between any parenthesis or NOT after any keyword.
Opening curly braces on same line for: while, do, for, if, namespace keywords. Not for classes, structs and typedefs.

Use textual operators instead of the sadly enough common operators:
not_eq instead of !=
not instead of !
and instead of &&
bitand instead of &
and_eq instead of &=
or instead of ||
bitor instead of |
or_eq instead of |=


Always put comment at closing brace for function / method for the name, like:

} // my_func

Except for very short functions not expected to grow.

Single line if / while / for statements are allowed, like:
if(isThisTrue)
	YesItWas+++;

Use with care of course, if you have a macro...think first.

Think of minimal scope for variable declaration.

Try to put related code/functions/method next to each other, like sections in a module. Don't mix unrelated code.

Prefer Qt classes over STL.

If there are any questions about these recommended guidlines, please don't hesitate to contact me.
Don't hesitate to let me know if I break my own recommendations...or better up, correct it.
