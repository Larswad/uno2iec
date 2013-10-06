#ifndef GLOBAL_DEFINES_HPP
#define GLOBAL_DEFINES_HPP

typedef unsigned long ulong;

// Define this if you want no logging to host enabled. Saves valuable space in flash.
//#define NO_LOGGING

#define CONSOLE_DEBUG
//#define DEBUGLINES

// Define this if you want to include the support for the MAX7219 display library in this project.
// What it does is showing loading progress on the display and some nice scrolling of the filename and other
// stuff on the display. The display hardware can be bought here at dx.com:
// http://dx.com/p/max7219-dot-matrix-module-w-5-dupont-lines-184854
#define USE_LED_DISPLAY

// The IEC bus pin configuration on the Arduino side (Deprecated: This will be defined from host side).
#define DEFAULT_ATN_PIN 5
#define DEFAULT_DATA_PIN 3
#define DEFAULT_CLOCK_PIN 4
#define DEFAULT_SRQIN_PIN 6
#define DEFAULT_RESET_PIN 7

#endif // GLOBAL_DEFINES_HPP
