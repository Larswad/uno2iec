#include "log.h"
#include <stddef.h>

#ifndef NO_LOGGING

const struct {
	const char abbreviated;
	const char *string;
} facilities[] PROGMEM = { FAC_MAIN, "MAIN", FAC_IEC, "IEC", FAC_IFACE, "IFACE" };

static const char siwe[] PROGMEM = "SIWE";

void registerFacilities(void)
{
	char strBuf[25];
	for(byte i = 0; i < sizeof(facilities) / sizeof(facilities[0]); ++i) {
		sprintf_P(strBuf, "!%c", pgm_read_byte(&facilities[i].abbreviated));
		strcat_P(strBuf, facilities[i].string);
		COMPORT.print(strBuf);
		COMPORT.print('\r\n');
	}
	COMPORT.flush();
} // registerFacilities


void Log(byte severity, char facility, char* msg)
{
	char strBuf[4];
	sprintf_P(strBuf, "D%c%c", pgm_read_byte(siwe + severity), facility);
	// NOTE: Queueing possible, polling of queue could be handled (called) from 'loop()'.
	COMPORT.print(strBuf);
	COMPORT.print(msg);
	COMPORT.print('\r\n');
} // Log

#endif
