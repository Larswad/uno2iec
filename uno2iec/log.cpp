#include "Log.h"

const struct {
	char abbreviated;
	char* string;
} facilities[] = { FAC_MAIN, "MAIN", FAC_IEC, "IEC", FAC_IFACE, "INTERFACE" };

static const char siwe[] = "SIWE";

void registerFacilities(void)
{
	char strBuf[25];
	for(byte i = 0; i < sizeof(facilities) / sizeof(facilities[0]); ++i) {
		sprintf(strBuf, "!%c%s", facilities[i].abbreviated, facilities[i].string);
		Serial.println(strBuf);
	}
} // registerFacilities


void Log(byte severity , char facility, char* msg)
{
	char strBuf[80];
	sprintf(strBuf, "D%c%c%s\r", siwe[severity], facility, msg);
	Serial.println(strBuf);
} // Log

