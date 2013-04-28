//
// Title        : UTILS
// Author       : Lars Pontoppidan Larsen
// Date         : February, 2006
// Target MCU   : Raspberry PI
//
// DESCRIPTION:
// This module contains functions for converting between ascii and binary
//
// Also contains delay functions
//
// DISCLAIMER:
// The author is in no way responsible for any problems or damage caused by
// using this code. Use at your own risk.
//
// LICENSE:
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//

#include "util.hpp"

// Tenths. This array could be moved to progmem
unsigned long tenths_tab[10] = {1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1};

// Writes an unsigned long as 13 ascii decimals:
//
//   Format: "x.xxx.xxx.xxx"
//   Leading zeros are written as spaces
//
void long2ascii(char *target, unsigned long value)
{
  unsigned char p, pos=0;
  unsigned char numbernow=0;

  for(p = 0;p < 10; p++) {

    if(p == 1 or  p == 4 or p == 7) {
      if (numbernow)
        target[pos] = '.';
      else
        target[pos] = ' ';

      pos++;
    }

    if (value < tenths_tab[p]) {
      if (numbernow)
        target[pos] = '0';
      else
        target[pos] = ' ';
    }
    else {
      target[pos] = '0';
      while(value >= tenths_tab[p]) {
        target[pos]++;
        value -= tenths_tab[p];
      }
      numbernow = 1;
    }
    pos++;
  }
}



// Writes a unsigned short as 5 ascii decimals, 0 padded
//
//   Example "00034"
//
void short2ascii(char *target, unsigned short value)
{
  unsigned char p;

  for(p = 0; p < 5; p++) {
    target[p] = '0';
    while(value >= tenths_tab[p+5]) {
      target[p]++;
      value -= tenths_tab[p+5];
    }
  }
}


// Converts ascii to an 32 bit unsigned value.
//
// Skips initial spaces and converts until a nonnumeral is encountered.
//
unsigned long ascii2long(char* dest)
{
  uchar i = 0;
  ulong value = 0;

  /* Look for first non-space */
  while(dest[i] == ' ')
    i++;

  /* Convert numerals */
  while((dest[i] >= '0') and (dest[i] <= '9')) {
    value *= 10;
    value += (dest[i] - '0');
    i++;
  }

  return value;
}


// alternative:
#include <sys/time.h>
void udelay(long us)
{
  struct timeval current;
  struct timeval start;

  gettimeofday(&start, NULL);
  do {
    gettimeofday(&current, NULL);
  } while( ( current.tv_usec * 1000000 + current.tv_sec ) - ( start.tv_usec * 1000000 + start.tv_sec ) < us );
} // udelay


// DELAY FUNCTIONS

#define LOOPS_PER_MS (F_CPU/1000/4)  //!< Loops pr ms

// Spin for ms milliseconds.
//
void ms_spin(unsigned short ms)
{
  Q_UNUSED(ms);
  //  delay(ms);
}

void us_spin(unsigned char us)
{
  Q_UNUSED(us);
  //  delayMicroseconds(us);
}
