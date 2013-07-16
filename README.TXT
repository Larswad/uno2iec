														UNO2IEC ReadMe


DISCLAIMER:
The author is in no way responsible for any problems or damage caused by
using this code. Use at your own risk.

LICENSE:
This code is distributed under the GNU Public License
which can be found at http://www.gnu.org/licenses/gpl.txt



================================================================================

Description
------------
The UNO2IEC device simulates a 1541 drive and enables loading and saving
programs to MMC or SD flash media with a CBM. The FAT16 and FAT32 filesystems
are supported on raspberry as as a server via serial communication.

Target device
-------------
Atmel ATMega328 (Arduino Uno) at ~16 MHz, 32KB flash, 2 KB SRAM, 1KB EEPROM.


Contact
-------
This project was created by:
		Lars Wadefalk, email: lars@wadeefalk.se

Project homepage:
		http://TODO.com


Hardware setup
--------------

How to build
------------
Open the PC/Raspberry project in Qt creator and build either release or debug.

Files in release:
-----------------
README.TXT
notes.txt
changes.txt
license.txt
./		Under repo root is the PC/Raspberry Qt project files.
uni2iec/	All files under here is the arduino sketch.
