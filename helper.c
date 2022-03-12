
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <ctype.h>

#ifndef _MSC_VER
#include <unistd.h>
#include <pwd.h>
#include <sys/ioctl.h>
#endif

#ifdef ENABLE_SSL
#include <openssl/x509.h>
#endif

#include "helper.h"
#include "file.h"
#include "os_specific.h"

#define SPEED_READER_BUFSIZE 1024

struct terminalDimension td;

static inline void logprintf_line (FILE *stream, char *color_code, char *prefix, char *formatString, va_list va_alist) {
    bool isColored = shouldColorOutput();
    if (isColored) {
        fprintf(stream, "%s[%s] - ", color_code, prefix);
    }
    else {
        fprintf(stream, "[%s] - ", prefix);
    }
    vfprintf(stream, formatString, va_alist);
    if (isColored) {
        fprintf(stream, "%s\n", KNRM);
    }
    else {
        fprintf(stream, "\n");
    }
}

void logprintf(int logLevel, char* formatString, ...) {
    va_list va_alist;
    struct xdccGetConfig* cfg = getCfg();

    if (cfg->logLevel == LOG_QUIET) {
        return;
    }

    va_start(va_alist, formatString);

    switch (logLevel) {
    case LOG_INFO:
        if (cfg->logLevel >= LOG_INFO) {
            logprintf_line(stdout, KGRN, "Info", formatString, va_alist);
        }
        break;
    case LOG_WARN:
        if (cfg->logLevel >= LOG_WARN) {
#ifdef _MSC_VER
            logprintf_line(stdout, KYEL, "Warning", formatString, va_alist);
#else
            logprintf_line(stderr, KYEL, "Warning", formatString, va_alist);
#endif
        }
        break;
    case LOG_ERR:
        if (cfg->logLevel >= LOG_ERR) {
#ifdef _MSC_VER
            logprintf_line(stdout, KRED, "Error", formatString, va_alist);
#else
            logprintf_line(stderr, KRED, "Error", formatString, va_alist);
#endif
        }
        break;
    default:
        DBG_WARN("logprintf called with unknown log-level. using normal logging.");
        vfprintf(stdout, formatString, va_alist);
        fprintf(stdout, "\n");
        break;
    }

    va_end(va_alist);
}

struct TextReaderContext {
    sds content;
};

static void TextReaderCallback (void *buffer, unsigned int bytesRead, void *ctx) {
    char *buf = buffer;
    struct TextReaderContext *context = ctx;
    buf[bytesRead] = (char) 0;
    context->content =  sdscatprintf(context->content, "%s", buf);
}

sds readTextFile(char *filePath) {
    struct TextReaderContext context;
    context.content = sdsnew("");

    readFile(filePath, TextReaderCallback, &context);

    return context.content;
}

void createRandomNick(int nickLen, char *nick) {
    char *possibleChars = "abcdefghiklmnopqrstuvwxyzABCDEFGHIJHKLMOPQRSTUVWXYZ";
    size_t numChars = strlen(possibleChars);
    int i;

    if (nick == NULL) {
        DBG_WARN("nick = NULL!");
        return;
    }

    for (i = 0; i < nickLen; i++) {
        nick[i] = possibleChars[rand_range(0, numChars - 1)];
    }
}

struct terminalDimension *getTerminalDimension() {
#if _MSC_VER

    HANDLE console;
    CONSOLE_SCREEN_BUFFER_INFO info;
    short rows = 0;
    short columns = 0;
    /* Create a handle to the console screen. */
    console = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        0, NULL);

    /* Calculate the size of the console window. */
    GetConsoleScreenBufferInfo(console, &info);
    CloseHandle(console);

    td.cols = info.srWindow.Right - info.srWindow.Left + 1;
    td.rows = info.srWindow.Bottom - info.srWindow.Top + 1;
#else
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    td.rows = w.ws_row;
    td.cols = w.ws_col;
#endif
    return &td;
}

void printProgressBar(const int numBars, const double percentRdy) {
    const int NUM_BARS = numBars;
    int i = 0;

    putchar('[');

    for (i = 0; i < NUM_BARS; i++) {
        if (i < (int) (NUM_BARS * percentRdy)) {
            putchar('#');
        }
        else {
            putchar('-');
        }
    }

    putchar(']');
}

int printSize(irc_dcc_size_t size) {
    char *sizeNames[] = {"Byte", "KByte", "MByte", "GByte", "TByte", "PByte"};

    double temp = (double) size;
    unsigned int i = 0;

    while (temp > 1024) {
        temp /= 1024;
        i++;
    }

    int charsPrinted = 0;

    if (i >= (sizeof (sizeNames) / sizeof (char*))) {
        charsPrinted = printf("%" IRC_DCC_SIZE_T_FORMAT " Byte", size);
    }
    else {
        charsPrinted = printf("%0.3f %s", temp, sizeNames[i]);
    }

    return charsPrinted;
}

uint64_t pow_uint64 (uint64_t number, uint64_t exp) {
    uint64_t ret = 1;
    
    for (int i = 0; i < exp; i++) {
        ret *= number;
    }
	
    return ret;
}

int printETA(double seconds) {
    int charsPrinted = 0;
    if (seconds <= 60) {
        charsPrinted = printf("%.0fs", seconds);
    }
    else {
        double mins = seconds / 60;
        double hours = mins / 60;
        double remainMins = mins - ((unsigned int) hours) * 60;
        double days = hours / 24;
        double remainHours = hours - ((unsigned int) days) * 24;
        double remainSeconds = seconds - ((unsigned int) mins) *60;

        if (days >= 1) {
            charsPrinted += printf("%.0fd", days);
        }

        if (remainHours >= 1) {
            charsPrinted += printf("%.0fh", remainHours);
        }

        charsPrinted += printf("%.0fm%.0fs", remainMins, remainSeconds);
    }
    return charsPrinted;
}

static void saveCurrentSpeed(struct dccCurrentSpeed *currentSpeedStruct, irc_dcc_size_t curSpeed) {
    currentSpeedStruct->curSpeed[currentSpeedStruct->speedIndex] = curSpeed;
    currentSpeedStruct->speedIndex = (currentSpeedStruct->speedIndex + 1) % NUM_AVERAGE_SPEED_VALUES;

    if (!currentSpeedStruct->isSpeedArrayFilled) {
        if (currentSpeedStruct->speedIndex == 0) {
            currentSpeedStruct->isSpeedArrayFilled = true;
        }
    }
}

static irc_dcc_size_t getAverageSpeed(struct dccCurrentSpeed *currentSpeedStruct, irc_dcc_size_t curSpeed) {
    if (currentSpeedStruct->isSpeedArrayFilled) {
        irc_dcc_size_t averageSpeed = 0;

        for (int i = 0; i < NUM_AVERAGE_SPEED_VALUES; i++) {
            averageSpeed += currentSpeedStruct->curSpeed[i];
            if (i > 0) {
                averageSpeed /= 2;
            }
        }

        return averageSpeed;
    } else {
        return curSpeed;
    }
}

void outputProgress(struct dccDownloadProgress *progress) {
    struct terminalDimension *terminalDimension = getTerminalDimension();
    /* see comments below how these "numbers" are calculated */
    int progBarLen = terminalDimension->cols - (8 + 14 + 1 + 14 + 1 + 14 + 3 + 13 /* +1 for windows...*/);

    progress->sizeLast = progress->sizeNow;
    progress->sizeNow = progress->sizeRcvd;
    irc_dcc_size_t curSpeed  = progress->sizeNow - progress->sizeLast;

    irc_dcc_size_t averageSpeed = 0;

    if (progress->sizeRcvd > 0) {
        saveCurrentSpeed(&(progress->curSpeed), curSpeed);
        averageSpeed = getAverageSpeed(&(progress->curSpeed), curSpeed);
        progress->averageSpeed = averageSpeed;
    }

    double curProcess = (progress->completeFileSize == 0) ? 0 : ((double)progress->sizeRcvd / (double) progress->completeFileSize);

    int printedChars = progBarLen + 2;

    printProgressBar(progBarLen, curProcess);
    /* 8 chars -->' 75.30% ' */
    printedChars += printf(" %.2f%% ", curProcess * 100);
    /* 14 chars --> '1001.132 MByte' */
    printedChars += printSize(progress->sizeRcvd);
    /* 1 char */
    printedChars += printf("/");
    /* 14 chars --> '1001.132 MByte' */
    printedChars += printSize(progress->completeFileSize);
    /*printf (" , Downloading %s", tdp->fileName);*/
    /* 1 char */
    printedChars += printf("|");
    /* 14 chars --> '1001.132 MByte' */
    printedChars += printSize(averageSpeed);
    /* 3 chars */
    printedChars += printf("/s|");

    /*calc ETA - max 13 chars */
    irc_dcc_size_t remainingSize = progress->completeFileSize - progress->sizeRcvd;
    if (remainingSize > 0 && averageSpeed > 0) {
        double etaSeconds = ((double) remainingSize / (double)averageSpeed);
        printedChars += printETA(etaSeconds);
    }
    else {
        printedChars += printf("---");
    }

    /* fill remaining columns of terminal with spaces, in ordner to clean the output... */

    int j;
    for (j = printedChars; j < terminalDimension->cols - 1; j++) {
        printf(" ");
    }
}

irc_dcc_size_t getSizeOf(unsigned int value, char *size) {
    char *sizeNames[] = {"Byte", "KByte", "MByte", "GByte", "TByte", "PByte"};
    int foundSize = -1;
	
    for (int i = 0; i < (sizeof (sizeNames) / sizeof (char*)); i++) {
        if (str_equals(size, sizeNames[i])) {
            foundSize = i;
            break;
        }
    }
	
    if (foundSize == -1) {
        return -1;
    } else {
        return value * pow_uint64(1024L, foundSize);
    }
}

void setMaxTransferSpeed(struct xdccGetConfig *config, sds value) {
    int val = 0;
    char size[SPEED_READER_BUFSIZE+1];
    memset(size, 0, sizeof(size));

#ifdef _MSC_VER
    int ret = sscanf_s(value, "%d%100s", &val, size, SPEED_READER_BUFSIZE);
#else
    int ret = sscanf(value, "%d%100s", &val, size);
#endif
    if (ret == 2) {
       irc_dcc_size_t maxSpeed = getSizeOf(val, size);
       if (maxSpeed > 0) {
          config->maxTransferSpeed = maxSpeed;
       } else {
          config->maxTransferSpeed = NO_SPEED_LIMIT;
       }
    } else {
         config->maxTransferSpeed = NO_SPEED_LIMIT;
    }
}

void setDelay(struct xdccGetConfig* config, sds value) {
    unsigned long val = 0;

#ifdef _MSC_VER
    sscanf_s(value, "%lu", &val);
#else
    sscanf(value, "%lu", &val);
#endif

    DBG_OK("setting delay to %lu", val);

    config->sendDelay = Calloc(1, sizeof(struct xdccSendDelay));
    config->sendDelay->sendDelayInSecs = val;
    config->sendDelay->timeToSendCommand = time(NULL) + val;
}

#ifdef ENABLE_SSL

static void print_validation_errstr(long verify_result) {
    logprintf(LOG_ERR, "There was a problem with the server certificate:");

    switch (verify_result) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        logprintf(LOG_ERR, "Unable to locally verify the issuer's authority.");
        break;
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
        logprintf(LOG_ERR, "Self-signed certificate encountered.");
        break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
        logprintf(LOG_ERR, "Issued certificate not yet valid.");
        break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
        logprintf(LOG_ERR, "Issued certificate has expired.");
        break;
    default:
        logprintf(LOG_ERR, "  %s", X509_verify_cert_error_string(verify_result));
    }
}

int openssl_check_certificate_callback(int verify_result, X509_STORE_CTX *ctx) {
    X509* cert = X509_STORE_CTX_get_current_cert(ctx);
    struct xdccGetConfig *cfg = getCfg();
    
    if (cert == NULL) {
        logprintf(LOG_ERR, "Got no certificate from the server.");
        return 0;
    }

    char *subj = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
    char *issuer = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
    
    logprintf(LOG_INFO, "Got the following certificate with");
    logprintf(LOG_INFO, "%s", subj);
    logprintf(LOG_INFO, "The issuer was:");
    logprintf(LOG_INFO, "%s", issuer);
    
    if (!verify_result) {
        print_validation_errstr(X509_STORE_CTX_get_error(ctx));
    }
    else {
        logprintf(LOG_INFO, "This certificate is trusted");
    }
    
    free(subj);
    free(issuer);
    
    if (cfg_get_bit(cfg, ALLOW_ALL_CERTS_FLAG)) {
        return 1;
    }

    return verify_result;
}

#endif
