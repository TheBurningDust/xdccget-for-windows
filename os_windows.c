#include "os_specific.h"
#include "helper.h"
#include "hashing_algo.h"

#include <stdbool.h>
#include <windows.h>
#include <VersionHelpers.h>
#include <Shlobj.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

static HANDLE gDoneEvent = NULL;
static HANDLE hTimer = NULL;
static HANDLE hTimerQueue = NULL;

static void (*interrupt_handler) (int) = NULL;
static void (*alarm_handler) (int) = NULL;

static bool useColoredConsole = false;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType)
    {
    case CTRL_C_EVENT:
        if (interrupt_handler) {
            interrupt_handler(0);
        }
        return true;
    }
    return false;
}

static void setup_timer_event() {

    // Use an event object to track the TimerRoutine execution
    gDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == gDoneEvent)
    {
        logprintf(LOG_ERR, "CreateEvent failed (%d)\n", GetLastError());
        exitPgm(EXIT_FAILURE);
    }

    // Create the timer queue.
    hTimerQueue = CreateTimerQueue();
    if (NULL == hTimerQueue)
    {
        logprintf(LOG_ERR, "CreateTimerQueue failed (%d)\n", GetLastError());
        exitPgm(EXIT_FAILURE);
    }
}

static VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    if (lpParam == NULL)
    {
        logprintf(LOG_WARN, "TimerRoutine lpParam is NULL\n");
    }
    else
    {
        alarm_handler(0);
    }
}

static void create_timer_queue_timer(int seconds) {
    int arg = 123;
    // Set a timer to call the timer routine in 1 seconds.
    
    if (seconds > 0) {
        if (!CreateTimerQueueTimer(&hTimer, hTimerQueue,
            (WAITORTIMERCALLBACK)TimerRoutine, &arg, seconds * 1000, 0, 0))
        {
            logprintf(LOG_ERR, "CreateTimerQueueTimer failed (%d)\n", GetLastError());
            exitPgm(EXIT_FAILURE);
        }
    }
    else if (seconds == 0) {
        if (!DeleteTimerQueueTimer(hTimerQueue, hTimer, NULL)) {  
            logprintf(LOG_ERR, "DeleteTimerQueueTimer failed (%d)\n", GetLastError());
            exitPgm(EXIT_FAILURE);
        }
        if (!DeleteTimerQueueEx(hTimerQueue, NULL)) {
            logprintf(LOG_ERR, "DeleteTimerQueueEx failed (%d)\n", GetLastError());
            exitPgm(EXIT_FAILURE);
        }
    }
}

const char* getPathSeperator() {
	return "\\";
}

const char* getHomeDir() {
    WCHAR path[MAX_PATH];
    size_t i;
	char *cpath = (char*) malloc(sizeof(char) * MAX_PATH);
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
		wcstombs_s(&i, cpath, MAX_PATH, path, MAX_PATH);
		return (const char*) cpath;
	}
	return NULL;
}

void createInterruptHandler(void (*handler) (int)) {
    interrupt_handler = handler;
	SetConsoleCtrlHandler(CtrlHandler, TRUE);
}

void createAlarmHandler(void (*handler) (int)) {
    alarm_handler = handler;
    setup_timer_event();
}

void enableAlarm(int seconds) {
    create_timer_queue_timer(seconds);
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
    struct checksumThreadData* data = lpParam;
    sds md5ChecksumString = data->expectedHash;

    logprintf(LOG_INFO, "Verifying md5-checksum '%s'!", md5ChecksumString);

    HashAlgorithm* md5algo = createHashAlgorithm("MD5");
    uchar hashFromFile[16];

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

    return 0;
}


void startChecksumThread(sds md5ChecksumSDS, sds completePath) {
    struct checksumThreadData* threadData = Malloc(sizeof(struct checksumThreadData));
    threadData->completePath = completePath;
    threadData->expectedHash = md5ChecksumSDS;
    DWORD   dwThreadId;

    HANDLE hThread = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        MyThreadFunction,       // thread function name
        threadData,          // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadId);   // returns the thread identifier 

    if (hThread == NULL) {
        logprintf(LOG_ERR, "could not create thread for checksum verification!");
        exitPgm(EXIT_FAILURE);
    }
}

static double getWindowsVersion()
{
    double ret = 0;
    NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
    OSVERSIONINFOEXW osInfo;

    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");

    if (NULL != RtlGetVersion)
    {
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);
        RtlGetVersion(&osInfo);
        ret = osInfo.dwMajorVersion;
    }
    return ret;
}

void enableAnsiColorCodes() {
    double win_version = getWindowsVersion();

    if (win_version >= 10.0) {
        HANDLE stdoutHandle;
        DWORD outModeInit;

        DWORD outMode = 0;
        stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

        if (stdoutHandle == INVALID_HANDLE_VALUE) {
            exitPgm(GetLastError());
        }

        if (!GetConsoleMode(stdoutHandle, &outMode)) {
            exitPgm(GetLastError());
        }

        outModeInit = outMode;
        // Enable ANSI escape codes
        outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

        if (!SetConsoleMode(stdoutHandle, outMode)) {
            exitPgm(GetLastError());
        }

        useColoredConsole = true;
    }
}

bool shouldColorOutput() {
    return useColoredConsole;
}

long int rand_range(long int low, long int high) {
    long int randomNumber = 0;
    if (high == 0) {
        return 0;
    }

    if (BCryptGenRandom(NULL, &randomNumber, sizeof(randomNumber), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != STATUS_SUCCESS) {
        logprintf(LOG_ERR, "could not get rand seed from BCryptGenRandom!");
        exitPgm(EXIT_FAILURE);
    }

    if (randomNumber < 0) {
        randomNumber *= -1;
    }

    return (randomNumber % high + low);
}

void initRand() {}