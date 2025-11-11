#ifndef HELPER_H
#define HELPER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef ENABLE_SSL
    #include <openssl/ssl.h>
    #include <openssl/pem.h>
#endif

#include "sds.h"
#include "libircclient-include/strings_utils.h"
#include "xdccget.h"
#include "libircclient.h"
#include "hashmap.h"
#include "argument_parser.h"
#include "os_specific.h"

#define LOG_ERR   0
#define LOG_QUIET 1
#define LOG_WARN  2
#define LOG_INFO  3


/* ansi color codes used at the dbg macros for coloured output. */

#ifndef ENABLE_ANSI_COLORS
#define KNRM  ""
#define KRED  ""
#define KGRN  ""
#define KYEL  ""
#define KBLU  ""
#define KMAG  ""
#define KCYN  ""
#define KWHT  ""
#else
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#endif
/* define DBG-macros for debugging purposes if DEBUG is defined...*/

#ifdef DEBUG
#ifdef _MSC_VER
	#define DBG_MSG(color, stream, format, ...) do {\
            fprintf(stream, "%sDBG:%s \"", color, KNRM);\
		    fprintf(stream, format, ##__VA_ARGS__);\
		    fprintf(stream, "\" function: %s file: %s line: %d\n",(char*) __func__, (char*)__FILE__, __LINE__); fflush(stdout);} while(0)
	
	#define DBG_OK(format, ...) do {\
				DBG_MSG(KGRN, stdout, format, ##__VA_ARGS__);\
		    } while(0)
	#define DBG_WARN(format, ...) do {\
				DBG_MSG(KYEL, stdout, format, ##__VA_ARGS__);\
		    } while(0)
	#define DBG_ERR(format, ...) do {\
		    	DBG_MSG(KRED, stdout, format, ##__VA_ARGS__);\
		    	exitPgm(EXIT_FAILURE);\
			} while(0)
#else
    #define DBG_MSG(color, stream, format, ...) do {\
		        fprintf(stream, "%sDBG:%s \"", color, KNRM);\
		        fprintf(stream, format, ##__VA_ARGS__);\
		        fprintf(stream, "\" function: %s file: %s line: %d\n",(char*) __func__, (char*)__FILE__, __LINE__);} while(0)

    #define DBG_OK(format, ...) do {\
				    DBG_MSG(KGRN, stdout, format, ##__VA_ARGS__);\
		        } while(0)
    #define DBG_WARN(format, ...) do {\
				    DBG_MSG(KYEL, stderr, format, ##__VA_ARGS__);\
		        } while(0)
    #define DBG_ERR(format, ...) do {\
		    	    DBG_MSG(KRED, stderr, format, ##__VA_ARGS__);\
		    	    exitPgm(EXIT_FAILURE);\
			    } while(0)
#endif
#else
	#define DBG_MSG(color, stream, format, ...) do {} while(0)
	#define DBG_OK(format, ...) do {} while(0)
	#define DBG_WARN(format, ...) do {} while(0)
	#define DBG_ERR(format, ...) do {} while(0)
#endif

#ifdef __GNUC__
    #define likely(x)       __builtin_expect(!!(x), 1)
    #define unlikely(x)     __builtin_expect(!!(x), 0)
#else
    #define likely(x)       (x)
    #define unlikely(x)     (x)
#endif

/* define macro for free that checks if ptr is null and sets ptr after free to null. */

#define FREE(X) \
do {\
	if ( (X != NULL) ) {\
		DBG_OK("freeing %p now", X);\
		free(( X ));\
		X = NULL;\
	}\
} while(0)

#define bitset_t uint64_t

typedef unsigned int sleeptime_t;
#define NO_SPEED_LIMIT 0

struct xdccSendDelay {
    time_t sendDelayInSecs;
    time_t timeToSendCommand;
};

struct xdccGetConfig {
    irc_session_t *session;
    uint32_t logLevel;
    struct dccDownload **dccDownloadArray;
    uint32_t numDownloads;
    irc_dcc_size_t maxTransferSpeed;
    struct xdccSendDelay* sendDelay;
    bitset_t flags;
    
    char *ircServer;
    sds *channelsToJoin;
    sds targetDir;
    sds nick;
    sds login_command;
    sds listen_ip;
    uint16_t listen_port;
    char *args[3];
    
    uint32_t numChannels;
    uint16_t port;
};

#define OUTPUT_FLAG               0x01
#define ALLOW_ALL_CERTS_FLAG      0x02
#define USE_IPV4_FLAG             0x03
#define USE_IPV6_FLAG	          0x04
#define VERIFY_CHECKSUM_FLAG      0x05
#define SENDED_FLAG               0x06
#define ACCEPT_ALL_NICKS_FLAG     0x07
#define DONT_CONFIRM_OFFSETS_FLAG 0x08
#define ON_CONNECT_EVENT_DONE     0x09


struct terminalDimension {
    int rows;
    int cols;
};

struct checksumThreadData {
	sds completePath;
	sds expectedHash;
};

struct dccDownloadContext {
    struct dccDownloadProgress *progress;
    struct file_io_t *fd;
};

static inline void clear_bit(bitset_t* x, int bitNum) {
    *x &= ~((bitset_t) 1 << bitNum);
}

static inline void set_bit(bitset_t* x, int bitNum) {
    *x |= ((bitset_t) 1 << (bitset_t) bitNum);
}

static inline int get_bit(bitset_t* x, int bitNum) {
    int bit = 0;
    bit = (*x >> bitNum) & 1L;
    return bit;
}

static inline void cfg_clear_bit(struct xdccGetConfig* config, int bitNum) {
    clear_bit(&config->flags, bitNum);
}

static inline void cfg_set_bit(struct xdccGetConfig* config, int bitNum) {
    set_bit(&config->flags, bitNum);
}

static inline int cfg_get_bit(struct xdccGetConfig* config, int bitNum) {
    return get_bit(&config->flags, bitNum);
}

void logprintf(int logLevel, char *formatString, ...);
/* Wrapper for malloc. Checks if malloc fails and exits pgm if it does. */
static inline void* Malloc(size_t size) {
    void *t = malloc(size);
    if (unlikely(t == NULL))
    {
        logprintf(LOG_ERR, "malloc failed. exiting now.\n");
        exit(EXIT_FAILURE);
    }
    
    return t;
}
/* Mallocs and then nulls the reserved memory.  */
static inline void* Safe_Malloc(size_t size) {
    void *t = Malloc(size);
    memset(t, 0, size);
    return t;
}
/* wraps calloc call. */
static inline void* Calloc(size_t numElements, size_t sizeOfElement) {
    void *t = calloc(numElements, sizeOfElement);
    if (unlikely(t == NULL))
    {
        logprintf(LOG_ERR, "calloc failed. exiting now.\n");
        exit(EXIT_FAILURE);
    }
    
    return t;
}

static inline sds getConfigDirectory() {
    sds configDir = sdscatprintf(sdsempty(), "%s%s%s%s", getHomeDir(), getPathSeperator(), ".xdccget", getPathSeperator());
    return configDir;
}

/* reads in the complete content of an text file and returns sds string. string need to be freed with sdsfree*/
sds readTextFile (char *filePath);

/* create a random nickname (e.g. a string) of nicklen chars. result is stored at nick.
   function does not malloc, so calling function has to reserve enough space at nick. */
void createRandomNick(int nickLen, char *nick);

struct terminalDimension *getTerminalDimension();

void printProgressBar(const int numBars, const double percentRdy);

int printSize (irc_dcc_size_t size);

irc_dcc_size_t getSizeOf(unsigned int value, char *size);

void outputProgress(struct dccDownloadProgress *tdp);

void setMaxTransferSpeed(struct xdccGetConfig *config, sds value);

static inline bool ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t len_str = strlen(str);
    size_t len_suf = strlen(suffix);
    if (len_suf > len_str) return false;
    return strcmp(str + (len_str - len_suf), suffix) == 0;
}

static inline sds get_custom_local_certs_folder() {
    return sdscatprintf(sdsempty(), "%s%s%s%s%s", getHomeDir(), getPathSeperator(), ".xdccget", getPathSeperator(), "local-trusted-certs");
}

static inline sds getListenIp(struct xdccGetConfig *config) {
    if (config->listen_ip) {
        return config->listen_ip;
    } else {
        return sdsnew("127.0.0.1");
    }
}

static inline uint16_t getListenPort(struct xdccGetConfig *config) {
    if (config->listen_port) {
        return config->listen_port;
    } else {
        return 0;
    }
}

#ifdef ENABLE_SSL
int openssl_check_certificate_callback(int preverify_ok, X509_STORE_CTX *ctx);
#endif

void setDelay(struct xdccGetConfig* config, sds value);

#endif
