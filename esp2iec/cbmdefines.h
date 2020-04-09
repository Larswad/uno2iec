#ifndef CBMDEFINES_H
#define CBMDEFINES_H

namespace CBM {

// 1541 RAM and ROM memory map definitions.
#define CBM1541_RAM_OFFSET 0
#define CBM1541_RAM_SIZE  (1024 * 2)
#define CBM1541_VIA1_OFFSET 0x1800
#define CBM1541_VIA1_SIZE 0x10
#define CBM1541_VIA2_OFFSET 0x1800
#define CBM1541_VIA2_SIZE 0x10
#define CBM1541_ROM_OFFSET 0xC000
#define CBM1541_ROM_SIZE (1024 * 16)

// Largest Serial byte buffer request from / to arduino.
#define MAX_BYTES_PER_REQUEST 256

// For every change of the serial protocol that makes a difference enough for incompitability, this number
// should be increased. That way the host side can detect whether the peers are compatible or not.
#define CURRENT_UNO2IEC_PROTOCOL_VERSION 2

// Device OPEN channels.
// Special channels.
enum IECChannels {
	READPRG_CHANNEL = 0,
	WRITEPRG_CHANNEL = 1,
	CMD_CHANNEL = 15
};
/*
#define READPRG_CHANNEL 0
#define WRITEPRG_CHANNEL 1
#define CMD_CHANNEL 15
*/

// Back arrow character code.
#define CBM_BACK_ARROW 0x5f
#define CBM_EXCLAMATION_MARKS "!!"
#define CBM_DOLLAR_SIGN '$'

const int MAX_CBM_SCREEN_ROWS = 25;
const int MAX_CBM_SCREEN_COLS = 40;

////////////////////////////////////////////////////////////////////////////////
//
// Error messages
//
// For detailed descriptions see: http://www.scribd.com/doc/40438/The-Commodore-1541-Disk-Drive-Users-Guide

typedef enum {
	ErrOK = 0,
    ErrFilesScratched,              // Files scratched response, not an error condition.
	ErrBlockHeaderNotFound = 20,
	ErrSyncCharNotFound,
	ErrDataBlockNotFound,
	ErrChecksumInData,
	ErrByteDecoding,
	ErrWriteVerify,
	ErrWriteProtectOn,
	ErrChecksumInHeader,
	ErrDataExtendsNextBlock,
	ErrDiskIdMismatch,
	ErrSyntaxError,
	ErrInvalidCommand,
	ErrLongLine,
	ErrInvalidFilename,
    ErrNoFileGiven,                 // The file name was left out of a command or the DOS does not recognize it as such.
                                    // Typically, a colon or equal character has been left out of the command
    ErrCommandNotFound = 39,        // This error may result if the command sent to command channel (secondary address 15) is unrecognizedby the DOS.
	ErrRecordNotPresent = 50,
	ErrOverflowInRecord,
	ErrFileTooLarge,
	ErrFileOpenForWrite = 60,
	ErrFileNotOpen,
	ErrFileNotFound,
	ErrFileExists,
	ErrFileTypeMismatch,
	ErrNoBlock,
	ErrIllegalTrackOrSector,
	ErrIllegalSystemTrackOrSector,
	ErrNoChannelAvailable = 70,
	ErrDirectoryError,
	ErrDiskFullOrDirectoryFull,
    ErrIntro,                       // power up message or write attempt with DOS mismatch
    ErrDriveNotReady,               // typically in this emulation could also mean: not supported on this file system.
    ErrSerialComm = 97,             // something went sideways with serial communication to the file server.
    ErrNotImplemented = 98,         // The command or specific operation is not yet implemented in this device.
	ErrUnknownError = 99,
	ErrCount
} IOErrorMessage;

} // namespace CBM

#endif // CBMDEFINES_H
