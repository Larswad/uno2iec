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
programs between a CBM and any "current" folder on the host system. The host system location can be a network share,
an USB or SD flash media, SSD or any hard disk that is supported by a Windows, Linux or Mac operating systems.

The direct commununication with the CBM is an arduino (uno or similar) communicating over a regular serial line
with the media host system. The serial line can be either the gpio pin mapped serial port on the arduino or the
serial over USB port. In case using the USB-over-serial port, the benefit is automatic reset of the arduino when the
host connects to it.

Note: As of writing the project does not yet support any turboloaders. There are however plans or ideas to provide
this support. It should be possible with some extra work of performance tuning for the optimum handling of the
serial interface between the arduino and the host.

The project inherits both code and ideas from the MMC2IEC project, originally written by
Jan Derogee's and Lars Pontoppidan. Their original work is great and highly respected.

The code in this project however, has been subject to heavy redesign. While being ported to C++ the main idea is to let the
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
Important: The pins you use on the arduino side should be configured in the global_defines.h header.

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
raspbian image is the suggested linux to use on Pi, it needs Qt build and runtime support.
For more info about how to prepare a raspberry pi properly for external serial communication, read the notes.txt file.
There are a few details when it comes to releasing the serial port from the kernel log.


How to build
------------
Open the PC/Raspberry project (.pro) file in Qt creator and build either as release or debug.
For windows, move back to the projects page and untick the "Shadow build" option. This puts object and executable
files in release and debug folders under the source folder.


REQUIRES: (external dependencies):
QextSerialPort project:
https://code.google.com/p/qextserialport/

The QextSerialPort project should be located on the same directory level as the "rpi2iec" project.

Optional: Max7219 driver library (modified by Lars Wadefalk to c++ with support for progress bar display and scroller).
Github project:
https://github.com/Larswad/arduino_max7219.git
To make use of the display features, enable the global define USE_LED_DISPLAY in the global_defines.h header.

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

The logging
-----------
Each line in the logging box gives information on what is going on between the host, the arduino and the CBM.
Each line has a date and time attached to the event. The event itself can take four different states:

S: Success, something went good.
I: Information, something happens, just providing the info
W: Warning, Something happened, might not be a problem
E: Error, Something went sideways.

The next field is a facility, it provides information about the part of the code that is giving the information.
If the facility contains an "R:" in front of it, it is the remote that has relayed the message to the host, the
facility in that case is one at the remote party.
The last field is the message part, informing about what has happened.


What else?
----------
Ideas and improvements are highly welcome. Please contribute to this project!
The retro machine community gains from it.

There are a few coding guidelines I would like to be followed (except for the ones who are obvious by just looking
at the present code).

The c++ files should have cpp and hpp file extensions (arduino environment however seems to prefer .h extensions for headers,
so let's honor that).
TABs instead of spaces are used (!), personally I use 2 indents (blanks) for each TAB, but use your own flavor.
Using TABs gives us more freedom.

There is NO limitation to 80 columns at all (or even 40 :-) ), but use common sense.
For splitted lines, operators are placed at the beginning of each new line to make code more clear (Qt style).

Try to use constants or enums rather than preprocessor defines, and try to keep methods/functions const, if possible or suitable.
Prefer passing variables by reference, when possible.

Use camelCase!

Blanks: Use them between operators, NOT between any parenthesis or NOT after any keyword.
Opening curly braces on same line for: while, do, for, if, namespace keywords. Not for classes, structs and typedefs.
Two linefeeds between each method or function, make it clear when a new function starts.
Use common sense for other linefeeds.


Use textual operators instead of the (sadly enough) commonly used operators, the textual ones make things clearer and less error prone:
not_eq instead of !=
not instead of !
and instead of &&
bitand instead of &
and_eq instead of &=
or instead of ||
bitor instead of |
or_eq instead of |=
xor instead of ^
and compl instead of ~

Try putting a comment at closing brace for function / method for the name, like:
} // my_func
Except for very short functions not expected to grow.
This is to make code walkthrough easier.


Single line if / while / for statements are allowed, like:
if(isThisTrue)
	yesItWas+++;

Use with care of course, if you have a macro or a more complex expression...think first.

Think of minimal scope for variable declaration.

Try to put related code/functions/method next to each other, like sections in a module. Don't mix unrelated code.

Prefer Qt classes over STL, because STL is good but Qt is better.

Commenting: Please do as much as you can. Comment WHY something is done, not preferably WHAT unless it is sort of
obscure. Do not comment the obvious.
It is nice to add a single line comment at end of long scope to indicate what scope it is, like:
} // my_function

Always Think about the DRY principle whenever writing code.
You are welcome to use templates but use them with care, if you find a need comment what is going on.

Patterns...well use them to power the code, but remember this is not an art contest.

Think about your platform when you code. Ardunio has little stack and memory, think optimization like in the old days.
Desktop has lot of memory and computing power; but do think a little about efficient code anyway, it never hurts.

If there are any questions or advice to change these recommended guidelines, please don't hesitate to contact me.
I am always open for change.

Don't hesitate to let me know if I break my own recommendations...or better up, correct it.
