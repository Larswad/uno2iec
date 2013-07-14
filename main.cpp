//
// Title        : MMC2IEC - main
// Author       : Lars Pontoppidan
// Date         : Jan. 2007
// Version      : 0.7
// Target MCU   : AtMega32(L) at 8 MHz
//
// CREDITS:
// --------
// The MMC2IEC application is inspired from Jan Derogee's 1541-III project for
// PIC: http://jderogee.tripod.com/
// This code is a complete reimplementation though, which includes some new
// features and excludes others.
//
// The FAT module was based on "FAT16/32 file system driver for ATMEL AVR"
// by Angelo Bannack, Giordano Bruno Wolaniuk, April 26, 2004 - which was based
// on the original library "FAT16/32 file system driver" from Pascal Stang.
//
// The SDCARD access module was developed in cooperation with Aske Olsson and
// Pascal Dufour.
//
// USAGE GUIDE:
// ------------
// The MMC2IEC device implements accessing FAT16/32 filesystems on MMC or SD
// flash media. The CBM IEC protocol is implemented and the kernel load and save
// routines on the C64 works, loading from either PRG, D64 or T64 files, and
// saving only to PRG type files in FAT.
//
// Selecting directories and images is possible through LOAD"xxx" commands
// issued on the CBM.
//
// This is a quick overview of the commands, say MMC2IEC is IEC device 8:
//
//   LOAD"<<",8      Reset SD card state. Do this if the SD card is exchanged.
//                   Read < as the petscii back-arrow.
//                   If SDCARD_DETECT signals is available, this operation is
//                   rarely needed.
//
//
// In FAT mode: (the default mode)
//
//   LOAD"$",8            Gets directory listing, equivalent to LOAD".",8
//   LOAD"gamesdir",8     Enter the "gamesdir" directory, and get listing.
//   LOAD"..",8           Up one directory and get directory listing.
//   LOAD"tetris.prg",8   Loads the "tetris.prg" program file.
//   SAVE"example.prg",8  Save into "example.prg" which is a FAT file.
//   LOAD"disk.d64",8     Loads the disk.d64 image and enters D64 mode.
//   LOAD"tape.t64",8     Loads the tape.t64 image and enters T64 mode.
//
// In D64 mode:
//
//   Load "$", "*", wildcards, filenames works (almost) as espected on a 1541.
//   LOAD"<",8            (back-arrow). Escape D64 mode, and back to FAT mode.
//   SAVE"abc",8          Fools the CBM, but has no effect. Saves in D64 are
//                        not implemented
//
// In T64 mode:
//
//   Load "$", "*", wildcards, filenames works as if it was a D64.
//   LOAD"<",8            (back-arrow). Escape T64 mode, and back to FAT mode.
//   SAVE"abc",8          Fools the CBM, but has no effect.
//
//
// NOTE:
// -----
// The FILE NOT FOUND ERROR is used to message both the original meaning, but
// it is also used to inform if there is an error with the SD/MMC or filesystem.
// Try a LOAD"<<",8 to reset SD card.
//
//
// HARDWARE INFO:
// --------------
// All of these, except the SPI signals, can be configured in config.h
//
//   Hardware SPI is used for MMC/SD card access:
//     PB7   SCK, output
//     PB6   MISO, input
//     PB5   MOSI, output
//     PB4   SS, output
//
//   The IEC bus:
//     PC0   ATN, input
//     PC1   DATA, open collector emulation
//     PC2   CLOCK, open collector emulation
//
//   LEDS:
//     PA0   (active low) IEC communication going on
//     PA1   (active low) Fat dirty indicator (don't remove flash media)
//
//   DEVICE9 jumper
//     PA2   If jumped to GND, MMC2IEC acts as device 9, otherwise 8
//
//   SDCARD_DETECT
//     PD2   Must connect to GND when SD CARD is present in socket
//
//   SDCARD_WP
//     PD6   Must connect to GND when SD CARD is not write protected
//
// COMPILER INFO:
// --------------
// This code compiles with avr-gcc v.3.4.6
//
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//


#include "mainwindow.hpp"
#include <QApplication>

#ifdef CONSOLE_DEBUG
#include <QDebug>
#endif

#include <QThread>

#include "IEC_driver.hpp"

#include "d64driver.hpp"
#include "t64driver.hpp"
#include "m2idriver.hpp"


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

#ifdef CONSOLE_DEBUG
	// Say hi
	qDebug() << endl << "Welcome." << endl;
#endif

	a.setOrganizationName("MumboJumbo_Software"/*VER_COMPANYNAME_STR*/);
	a.setOrganizationDomain("DOMAIN");
	a.setApplicationName("Uno2IEC");

	MainWindow w;
	w.show();

	return a.exec();
}
