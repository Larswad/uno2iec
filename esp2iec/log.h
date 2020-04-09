#ifndef LOG_H
#define LOG_H

#include "global_defines.h"

#ifndef NO_LOGGING

#include <Arduino.h>

#define FAC_MAIN 'M'
#define FAC_IEC 'I'
#define FAC_IFACE 'F'

enum Severity { Success, Information, Warning, Error };
void registerFacilities(void);
void Log(byte severity , char facility, char* msg);

#else

#define registerFacilities()
#define Log(severity, facility, msg)

#endif // NO_LOGGING

#endif

