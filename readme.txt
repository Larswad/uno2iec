
                              MMC2IEC ReadMe


DISCLAIMER:
The author is in no way responsible for any problems or damage caused by
using this code. Use at your own risk.

LICENSE:
This code is distributed under the GNU Public License
which can be found at http://www.gnu.org/licenses/gpl.txt



================================================================================

Description
------------
The MMC2IEC device simulates a 1541 drive and enables loading and saving 
programs to MMC or SD flash media with a C64. The FAT16 and FAT32 filesystems
are supported on flash media.


Target device
-------------
Atmel ATMega32 at ~8 MHz.


Contact
-------
This project was created by:
    Lars Pontoppidan, email: larspontoppidan@gmail.com
   
Project homepage: 
    http://pontoppidan.info/lars/index.php?proj=mmc2iec

  
    
How to build
------------
The project can be built using AVR-GCC version 3.4.6, which can be installed 
with the WinAVR package on Windows PCs. Use make.

The project can be built with or without UART debugging, which is configured in
config.h. If this feature is not needed, it is recommended to build without 
UART debugging.

In config.h it is also possible to disable certain features, such as mechanical
card detect switch, write protect, device 9/8 jumper.

Pay attention to the F_CPU assignment in makefile. This value should match the 
clock frequency of the AVR reasonably. The F_CPU affects delay loops and the 
UART baud rate calculation.



AVR Fuses
---------
OFF: JTAG Interface Enabled;
ON:  Int. RC Osc. 8 MHz; Start-up time: 6 CL + 64 ms



Files in release:
-----------------  
readme.txt
changes.txt
license.txt
mmc2iec_dtv_schematic.png
mmc2iec_c64_schematic.png
mmc2iec.hex
mmc2iec.pnproj
mmc2iec.c   
interface.c 
interface.h 
IEC_driver.c
IEC_driver.h
d64driver.c 
d64driver.h 
t64driver.c 
t64driver.h 
util.c      
util.h      
uart.c      
uart.h      
fat.c       
fat.h       
fattime.c   
fattime.h   
buffers.c   
buffers.h
sdcard.c    
sdcard.h    
spi.c       
spi.h 
config.h
makefile    
