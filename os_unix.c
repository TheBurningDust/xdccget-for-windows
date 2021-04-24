#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <sys/types.h>
#include <pthread.h>

#include "os_specific.h"
#include "helper.h"
#include "hashing_algo.h"

static void init_signal(int signum, void (*handler) (int)) {
    struct sigaction act;
    int ret;

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);

    act.sa_handler = handler;
    act.sa_flags = SA_RESTART;

    ret = sigaction(signum, &act, NULL);
    if (ret == -1) {
        logprintf(LOG_ERR, "could not set up signal %d", signum);
        exitPgm(EXIT_FAILURE);
    }
}

static void* checksum_verification_thread(void* args) {
    struct checksumThreadData* data = args;
    sds md5ChecksumString = data->expectedHash;

    logprintf(LOG_INFO, "Verifying md5-checksum '%s'!", md5ChecksumString);

    HashAlgorithm* md5algo = createHashAlgorithm("MD5");
    uchar hashFromFile[md5algo->hashSize];

    getHashFromFile(md5algo, data->completePath, hashFromFile);
    uchar* expectedHash = convertHashStringToBinary(md5algo, md5ChecksumString);

    if (md5algo->equals(expectedHash, hashFromFile)) {
        logprintf(LOG_INFO, "Checksum-Verification succeeded!");
    }
    else {
        logprintf(LOG_WARN, "Checksum-Verification failed!");
    }

    FREE(expectedHash);
    freeHashAlgo(md5algo);
    sdsfree(data->expectedHash);
    sdsfree(data->completePath);
    FREE(data);

    return NULL;
}

const char* getPathSeperator() {
	return "/";
}

const char* getHomeDir() {
    struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;
	return homedir;
}

void createInterruptHandler(void (*handler) (int)) {
    init_signal(SIGINT, handler);
}

void createAlarmHandler(void (*handler) (int)) {
    init_signal(SIGALRM, handler);
}

void enableAlarm(int seconds) {
    alarm(seconds);
}

void startChecksumThread(sds md5ChecksumSDS, sds completePath) {
    struct checksumThreadData* threadData = Malloc(sizeof(struct checksumThreadData));
    threadData->completePath = completePath;
    threadData->expectedHash = md5ChecksumSDS;

    pthread_t threadID;

    pthread_create(&threadID, NULL, checksum_verification_thread, threadData);
}

void enableAnsiColorCodes() {

}