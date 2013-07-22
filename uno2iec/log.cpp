#include "log.h"

const struct {
	char abbreviated;
	char* string;
} facilities[] = { FAC_MAIN, "MAIN", FAC_IEC, "IEC", FAC_IFACE, "INTERFACE" };

static const char siwe[] = "SIWE";

void registerFacilities(void)
{
	char strBuf[25];
	for(byte i = 0; i < sizeof(facilities) / sizeof(facilities[0]); ++i) {
		sprintf(strBuf, "!%c%s\r", facilities[i].abbreviated, facilities[i].string);
		Serial.print(strBuf);
	}
} // registerFacilities


void Log(byte severity, char facility, char* msg)
{
	char strBuf[4];
	sprintf(strBuf, "D%c%c", siwe[severity], facility);
	// TODO: Queueing possible, polling of queue could be handled (called) from 'loop()'.
	Serial.print(strBuf);
	Serial.print(msg);
	Serial.print("\r");
} // Log
