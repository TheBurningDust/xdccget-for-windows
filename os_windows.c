#include "os_specific.h"

#include "helper.h"

#include <stdbool.h>
#include <windows.h>
#include <VersionHelpers.h>
#include <Shlobj.h>

static HANDLE gDoneEvent = NULL;
static HANDLE hTimer = NULL;
static HANDLE hTimerQueue = NULL;

static void (*interrupt_handler) (int) = NULL;
static void (*alarm_handler) (int) = NULL;

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
    if (!CreateTimerQueueTimer(&hTimer, hTimerQueue,
        (WAITORTIMERCALLBACK)TimerRoutine, &arg, seconds*1000, 0, 0))
    {
        logprintf(LOG_ERR, "CreateTimerQueueTimer failed (%d)\n", GetLastError());
        exitPgm(EXIT_FAILURE);
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

void startChecksumThread(sds md5ChecksumSDS, sds completePath) {
    // TODO: implement checksum thread for windows environment...

    sdsfree(md5ChecksumSDS);
    sdsfree(completePath);
}

static double getWindowsVersion()
{
    int ret = 0;
    NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
    OSVERSIONINFOEXW osInfo;

    *
        
        
        
        
        
        
        
        
        
        
        
        
        (FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");

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
    }
}