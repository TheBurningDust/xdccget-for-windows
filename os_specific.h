#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include "sds.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

const char* getPathSeperator();
const char* getHomeDir();

void createInterruptHandler(void (*handler) (int));
void createAlarmHandler(void (*handler) (int));
void enableAlarm(int seconds);

void startChecksumThread(sds md5ChecksumSDS, sds completePath);
void enableAnsiColorCodes();

#endif
