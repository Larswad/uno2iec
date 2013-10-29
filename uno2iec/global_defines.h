#ifndef GLOBAL_DEFINES_HPP
#define GLOBAL_DEFINES_HPP

typedef unsigned long ulong;

// Define this if you want no logging to host enabled at all. Saves valuable space in flash.
//#define NO_LOGGING

// Enable this for verbose logging of IEC and CBM interfaces.
//#define CONSOLE_DEBUG
// Enable this to debug the IEC lines (checking soldering and physical connections). See project README.TXT
//#define DEBUGLINES

// Define this if you want to include the support for the MAX7219 display library in this project.
// What it does is showing loading progress on the display and some nice scrolling of the filename and other
// stuff on the display. The display hardware can be bought cheap here at dx.com:
// http://dx.com/p/max7219-dot-matrix-module-w-5-dupont-lines-184854
//#define USE_LED_DISPLAY

// Define this if you want to configure a HC-06 bluetooth module connected to the arduino and make the communication
// with the commodore machine completely wireless. Defining this will configure the BT module in the main sketch.
//#define CONFIG_HC06_BLUETOOTH

// For serial communication.
#define DEFAULT_BAUD_RATE 115200
#define SERIAL_TIMEOUT_MSECS 1000

#endif // GLOBAL_DEFINES_HPP
