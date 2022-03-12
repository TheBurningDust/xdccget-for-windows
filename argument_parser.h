#ifndef ARGUMENT_PARSER_H
#define ARGUMENT_PARSER_H

#include "sds.h"
#include "helper.h"

struct dccDownload {
    sds botNick;
    sds xdccCmd;
    sds md5;
};

#define NUM_AVERAGE_SPEED_VALUES 8

struct dccCurrentSpeed {
    irc_dcc_size_t curSpeed[NUM_AVERAGE_SPEED_VALUES];
    int speedIndex;
    bool isSpeedArrayFilled;
};

struct dccDownloadProgress {
    irc_dcc_size_t completeFileSize;
    irc_dcc_size_t sizeRcvd;
    irc_dcc_size_t sizeNow;
    irc_dcc_size_t sizeLast;
    irc_dcc_size_t averageSpeed;
    sds completePath;
    struct dccCurrentSpeed curSpeed;
};

void parseArguments(int argc, char **argv, struct xdccGetConfig *args);

struct dccDownload* newDccDownload(char *botNick, char *xdccCmd);

void freeDccDownload(struct dccDownload *t);

struct dccDownloadProgress* newDccProgress(char *filename, irc_dcc_size_t complFileSize);

void freeDccProgress(struct dccDownloadProgress *progress);

void parseDccDownload (char *dccDownloadString, char **nick, char **xdccCmd);

sds* parseChannels(char *channelString, uint32_t *numChannels);

struct dccDownload** parseDccDownloads(char *dccDownloadString, unsigned int *numDownloads);

#endif
