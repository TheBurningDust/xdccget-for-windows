#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <stdbool.h>

#include "sds.h"

#ifndef _MSC_VER
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strdup(p) _strdup(p)
#endif

const char* getPathSeperator();
const char* getHomeDir();

void createInterruptHandler(void (*handler) (int));
void createAlarmHandler(void (*handler) (int));
void enableAlarm(int seconds);


void startChecksumThread(sds md5ChecksumSDS, sds completePath);
void enableAnsiColorCodes();
bool shouldColorOutput();

void initRand();
long int rand_range(long int low, long int high);

#endif
