#include "log.h"
#include <stddef.h>

#ifndef NO_LOGGING

const struct {
	prog_uchar abbreviated;
	prog_uchar string[6]; // <-- Set this to the longest one including \0
} facilities[] PROGMEM = { FAC_MAIN, "MAIN", FAC_IEC, "IEC", FAC_IFACE, "IFACE" };

static const char siwe[] PROGMEM = "SIWE";

void registerFacilities(void)
{
	char strBuf[25];
	for(byte i = 0; i < sizeof(facilities) / sizeof(facilities[0]); ++i) {
		sprintf_P(strBuf, (PGM_P)F("!%c"), pgm_read_byte(&facilities[i].abbreviated));
		strcat_P(strBuf, (PGM_P)facilities[i].string);
		Serial.print(strBuf);
		Serial.print('\r');
	}
	Serial.flush();
} // registerFacilities


void Log(byte severity, char facility, char* msg)
{
	char strBuf[4];
	sprintf_P(strBuf, (PGM_P)F("D%c%c"), pgm_read_byte(siwe + severity), facility);
	// NOTE: Queueing possible, polling of queue could be handled (called) from 'loop()'.
	Serial.print(strBuf);
	Serial.print(msg);
	Serial.print('\r');
} // Log

#endif
