#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

#define FAC_MAIN 'M'
#define FAC_IEC 'I'
#define FAC_IFACE 'F'

enum Severity { Success, Information, Warning, Error };
void registerFacilities(void);
void Log(byte severity , char facility, char* msg);

#endif

