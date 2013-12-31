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
The UNO2IEC device simulates a 1541 drive and enables loading and saving files or programs between a CBM and any "current" folder on the host system.
The host system's media location can be a network share, a USB or SD flash media, SSD or any hard disk that is supported by a Windows, Linux or Mac operating systems.

The direct commununication with the CBM is an Arduino (uno, nano, dumilanouve or similar) communicating over a regular serial line with the media host system. The serial line can be either the gpio pin mapped serial port on the arduino or the
serial over USB port (FTDI). In case using the USB-over-serial port, the advantage is the ability to trigger an automatic reset of the Arduino when the host connects to it.

Note: As of writing the project does not yet support any turboloaders. There are however plans and ideas to provide this support. It should be possible with some extra work of performance tuning for the optimum handling of the
serial interface between the arduino and the host. The memory and Arduino processing capabilities are more than good enough.

This project has inherited both code and ideas from the MMC2IEC project, originally written by Jan Derogee and Lars Pontoppidan.
The sd2iec project has also been used as a reference for the IEC interface and commodore DOS protocols.
Their original work and achievements is great and also highly respected.

The code in this project however, has been subject to heavy redesign. Nothing of the original code is kept in its original state.
Everything has been ported to C++ and Qt, the code was refactored to object oriented programmering and fits nicely as such.

The main idea is to let the old commodore targeted media be transferred from a PC host rather than just a MMC or SD card.
Therefore the design of this project required the IEC protocol between the CBM and the emulated 1541 to be refactored into having divided responsibilities between the Arduino (as a low level middlehand) and the PC host as a media-tank. In the MMC2IEC and SD2IEC projects everything was written to work and function on one single CPU (or system).
A secondary proprietary RS232-serial based protocol between the Arduino is utilized to be able to transfer the actual media to
the CBM. This protocol was designed for compactness and performance in order to maintain the IEC transfer speed.


Target device
-------------
Atmel ATMega328 (Arduino Uno) at ~16 MHz, 32KB flash, 2 KB SRAM, 1KB EEPROM.


Contact
-------
This project was created by:
		Lars Wadefalk, email: lars@wadefalk.se

Project homepage:
		http://TODO.com


Hardware setup and variations
-----------------------------
Arduino Uno/Nano: ATmega 16U2 328 microcontroller @ 16Mhz, 32KB flash, SRAM 2KB, EEPROM 1 KB.
Connect at least ATN, CLOCK, DATA and GND pin to the IEC bus on the CBM. The RESET and SRQIN (latter one is unused on the C64) are optional to wire, however if you don't connect the RESET pin, the HAS_RESET_LINE definition should be commented out in the file global_defines.h. The SRQ_IN pin may be later supported for C128 compatibility.

The Arduino UNO can be bought very cheap at dx.com:
http://dx.com/p/uno-r3-development-board-microcontroller-mega328p-atmega16u2-compat-for-arduino-blue-black-215600

Same goes for the Arduino NANO which is even more compact and cheap:
http://dx.com/p/nano-v3-0-avr-atmega328-p-20au-module-board-usb-cable-for-arduino-118037

Optional: LED matrix with MAX7219 controller.
Can be bought cheap at dx.com with 5 dupont lines:
http://dx.com/p/max7219-dot-matrix-module-w-5-dupont-lines-184854

Whether or not using the LED matrix when compiling, either define or comment out the USE_LED_DISPLAY macro in the global_defines.h header in the uno2iec project.

Optional: HC-06 compatible Bluetooth module. This will provide the possibility to make your arduino completely wirelessly communicating with the PC media host. The JY-MCU is cheap, around 8 bucks and is pretty easy to configure:
http://dx.com/p/jy-mcu-arduino-bluetooth-wireless-serial-port-module-104299

See separate section further down for how to configure and use the LED Matrix and Bluetooth communication.

Cables:
-------
You can use the following jumper wires bought cheaply at dx.com if you're using a UNO:
http://dx.com/p/breadboard-jumper-cable-wires-for-electronic-diy-65-cable-pack-121345
If you have an Arduino Mini needs female cables instead, use the following:
http://dx.com/p/single-port-female-to-female-jumper-wire-set-50-pack-20cm-length-55454

Either connect Arduino to a PC, MAC. Media host side project can be compiled on either Linux, Windows or Mac desktop.
It works on the Raspberry Pi as a host as well. Connect two serial pins (RX, TX, twisted of course) on the PI to the arduino. A third pin may also be connected with the purpose to reset the Arduino from the PI side (the PI uses the wiringPi project for GPIO access). The raspbian image is the suggested linux to use on Pi, it needs Qt build and runtime support.
For more info about how to prepare a raspberry pi properly for external serial communication, read the notes.txt file.
There are a few details when it comes to releasing the serial port from the kernel log.


How to build
------------
Getting the code:
The latest committed code is fetched from github using the following git command line:

git clone https://github.com/Larswad/uno2iec.git

OR you can download the code as a zip file from github at the following URL:
https://github.com/Larswad/uno2iec/archive/master.zip

To be sure to get a version that is more tested and stable use the latest release (tagged) version, refer to the available releases at github:
https://github.com/Larswad/uno2iec/releases

Open the PC (or if raspberry pi) project (.pro) file in Qt creator and build either as release or debug.
For windows, move back to the projects page and untick the "Shadow build" option. This puts object and executable
files in release and debug folders under the source folder.

Build a single executable without dependencies:
-----------------------------------------------


REQUIREMENTS: (external dependencies):
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

Blanks: Use them between operators, NOT before or after any parenthesis.
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



Single line if / while / for statements are allowed, like:
if(isThisTrue)
	yesItWas+++;

Use with care of course, if you have a macro or a more complex expression...think first.

Think of minimal scope for variable declaration.

Try to put related code/functions/method next to each other, like sections in a module. Don't mix unrelated code.

Prefer Qt classes over STL, because STL is good but Qt is better.

Commenting: Please do as much as you can. Comment WHY something is done, not preferably WHAT unless it is sort of
obscure. Do not comment the obvious.
It is nice to add a single line comment at closing brace after end of long scope to indicate what scope it is, like:
} // myFunction
This is to make code browsing easier.


Always Think about the DRY principle whenever writing code.
You are welcome to use templates but use them with care, if you find a need comment what is going on.

Patterns...well use them to power the code, but remember this is not an art contest. Keep Things Simple.

Think about your target platform when you code. Ardunio has little stack and memory, think optimization like in the old days.
Desktop has lot of memory and computing power; but do think a little about efficient code anyway, it never hurts.

If there are any questions or advice to change these recommended guidelines, please don't hesitate to contact me.
I am always open for change.

Don't hesitate to let me know if I break my own recommendations...or better up, correct it.


Configuring the bluetooth module HC-06 ("JY-MCU") :
---------------------------------------------------
The blueooth module should be connected to the arduino RX/TX lines. Important here is that between the Arduino TX and the bluetooth
module RX-pin there should be a levelshifter from 5V to 3.3V to protect it, since the bluetooth module is operating at 3.3V and is not 5V tolerant.
This is easily built using a couple of resistors, or a cheap simple to use four-way level shifter can be bought at dx.com as well.
The Arduino RX can go directly to the bluetooth module's TX line. The bluetooth module must be powered with 3.3V from the Arduino's 3.3 output pin.
Connect a ground pin from the arduino to the bluetooth ground as well.

1. Before actually connecting the module RX/TX lines to the arduino, the software (sketch) has to be prepared and flashed to the board
in a special mode for a one-time software configuration of baudrate, PIN-code and device name.
Uncomment the CONFIG_HC06_BLUETOOTH define in the global_defines.h header (also set the baudrate definition in this header to the default one, 9600).
Compile and upload the sketch to the arduino board. Note than you can NOT upload the sketch over USB while the RX/TX lines are connected
to the bluetooth device! The USB-FTDI chip is disabled if the RX/TX lines are active on the board.

2. After successful upload of the sketch (that is compiled for configuration), remove the USB connector and connect the RX/TX lines to the board.
Now connect the USB to the arduino that now will perform the configuration of the device, this will go very quick, a matter of a few seconds.
While doing this, do not have the bluetooth module connected wirelessly to any device because the device will only accept configuration data when not
beeing paired or connected wirelessly.

3. Disconnect the USB cable again and remove the RX/TX lines again between arduino and bluetooth module.
Then comment out the CONFIG_HC06_BLUETOOTH definition again so that the code will be back to running in normal mode.
Now it is important to also restore the baudrate definition in the global_defines.h header to the acutal one that the module was
configured to use when the device was configured. This parameter was set with an AT command in the main sketch file's setup function.
The baudrate in the main sketch file should have configured the device to 57600 (but that may of course be set to something else, if desired).
Connect the USB to the Arduino, compile an upload the code again. The Arduino should now be in "normal" IEC emulation mode again.
4. Remove the USB connector and connect the bluetooth module's RX/TX lines again. Now power up the device again with the USB connection.

5. This should be enough to let the bluetooth module pair with the host PC and get the wireless communication working.
When pairing, use the correct PIN code and then the generic drivers for a bluetooth COM-port device should be installed automatically.
This has been sucessfully tested on Windows 7 but should work with XP and Linux as well.

6. You have to use the right COM-port when connecting the PC host application. When successfully connected, the module's red led stops flashing and
is kept constantly lit as long as connection is established. If failing to connect the Qt host application, try once or twice again by
clicking the "Reset Arduino" button in the UI. Do not forget to set the host application's baud rate in the settings dialog to the same
baud rate as the one that was configured for the bluetooth module.


Powering the Arduino using the CBM TAPE-cassette port.
------------------------------------------------------
The final step to get the Arduino IEC emulator completely wireless is to get rid of the need to power it from the PC USB connection.
The CBM's provides 5V power on the TAPE port. The current capability is well enough to power the Arduino and its peripherals.
Either slaughter a cable connector from an existing datasette TAPE deck (perhaps old and broke) or buy one 6 pin female edge connector.
The 5V line should go to the Arduino board's VIN line. The ground line should also be connected to the Arduino's GND pin.

This should be enough to power the device when you switch on your commodore machine.

Before soldering the end of the TAPE connector's cable, it is highly recommended to measure the wires with a multimeter to make sure
which one is the 5V and which is the ground. A simple mistake could ruin either the Arduino, or even worse your beloved vintage computer.
Use shrinking tube to shield any exposed soldered wires to avoid accidental short circuits.

Alternatively the arduino can be powered from either a battery pack or a cell-phone charger battery that carries a USB connection.
