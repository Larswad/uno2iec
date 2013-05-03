#include "Log.h"

const struct {
	char abbreviated;
	char* string;
} facilities[] = { 'M', "MAIN", 'I', "IEC" };

static const char siwe[] = "SIWE";

void registerFacilities(void)
{
	char strBuf[12];
	for(byte i = 0; i < sizeof(facilities) / sizeof(facilities[0]); ++i) {
		sprintf(strBuf, "!%c%s", facilities[i].abbreviated, facilities[i].string);
		Serial.println(strBuf);
	}
} // registerFacilities


void Log(byte severity , char facility, char* msg)
{
	char strBuf[80];
	sprintf(strBuf, "D%c%c%s", siwe[severity], facility, msg);
	Serial.println(strBuf);
} // Log

