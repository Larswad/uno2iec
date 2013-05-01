#ifndef LOG_H
#define LOG_H

#include <arduino.h>

enum Severity { Success, Information, Warning, Error };
void registerFacilities(void);
void Log(byte severity , char facility, char* msg);

#endif

