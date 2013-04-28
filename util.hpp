#ifndef UTIL_H
#define UTIL_H

//
// Title        : UTILS
// Author       : Lars Pontoppidan Larsen
// Date         : February, 2006
// Target MCU   : Raspberry PI
//

#include <qglobal.h>


#ifndef cbi
#define cbi(reg,bit)	reg &= ~(1<<bit)
#endif
#ifndef sbi
#define sbi(reg,bit)	reg |= (1<<bit)
#endif

#define INC_RING(var, bsize) if (var == ((bsize)-1)) var=0; else var++


void long2ascii(char *target, unsigned long value);
unsigned long ascii2long(char *dest);
void ms_spin(unsigned short ms);

// void DelayBigUs(unsigned short us);

void us_spin(unsigned char us);

void short2ascii(char *target, unsigned short value);

#endif
