/*
	xdccget -- download files from xdcc via cmd line
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <inttypes.h>

#include "helper.h"
#include "file.h"
#include "config.h"
#include "os_specific.h"

#define NICKLEN 24

#ifdef _MSC_VER
int usleep(DWORD usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * (__int64)usec);

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer != NULL) {
        SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
        WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
    }
    return 0;
}
#endif

static struct xdccGetConfig cfg;

static uint32_t numActiveDownloads = 0;
static uint32_t finishedDownloads = 0;
static struct dccDownloadContext **downloadContext = NULL;
static struct dccDownloadProgress *lastDownload = NULL;
static struct dccDownloadProgress *curDownload = NULL;

struct xdccGetConfig *getCfg() {
    return &cfg;
}

void doCleanUp() {
    uint32_t i;

    if (cfg.session)
        irc_destroy_session(cfg.session);

    for (i = 0; i < cfg.numChannels; i++) {
        sdsfree(cfg.channelsToJoin[i]);
    }

    if (cfg.dccDownloadArray) {
        for (i = 0; cfg.dccDownloadArray[i]; i++) {
            freeDccDownload(cfg.dccDownloadArray[i]);
        }
    }

    for (i = 0; i < cfg.numDownloads && downloadContext[i]; i++) {
        struct dccDownloadContext *current_context = downloadContext[i];
        struct dccDownloadProgress *current_progress = current_context->progress;

        if (current_progress != NULL) {
            bool finishedDownloading = current_progress->sizeRcvd == current_progress->completeFileSize;

            if (!finishedDownloading) {
                Close(current_context->fd);
                current_context->fd = NULL;
            }

            freeDccProgress(current_context->progress);
        }

        FREE(downloadContext[i]);
    }

    sdsfree(cfg.targetDir);
    sdsfree(cfg.nick);
    sdsfree(cfg.login_command);
    sdsfree(cfg.listen_ip);
    FREE(cfg.dccDownloadArray);
    FREE(cfg.channelsToJoin);
    FREE(downloadContext);
}

void exitPgm(int retCode) {
    doCleanUp();
    exit(retCode);
}

void interrupt_handler(int signum) {
    if (cfg.session && irc_is_connected(cfg.session)) {
        irc_cmd_quit(cfg.session, "Goodbye!");
    }
    else {
        exitPgm(0);
    }
}

void output_all_progesses() {
    unsigned int i;

    if (numActiveDownloads < 1) {
        printf("Please wait");
        if (cfg.sendDelay != NULL) {
            if (cfg.sendDelay->timeToSendCommand > time(NULL)) {
                time_t waitTime = cfg.sendDelay->timeToSendCommand - time(NULL);
                printf(" %lus", waitTime);
            }
        }
        printf(" until the download is started!\r");
    }
    else {
        for (i = 0; i < numActiveDownloads; i++) {
            outputProgress(downloadContext[i]->progress);

            if (numActiveDownloads != 1) {
                printf("\n");
            }
        }
    }

    if (numActiveDownloads == 1) {
        /* send \r so that we override this line the next time...*/
        printf("\r");
    }

    fflush(stdout);
}

void output_handler(int signum) {
    enableAlarm(1);
    cfg_set_bit(getCfg(), OUTPUT_FLAG);
}

static sds extractMD5(const char* string) {
    const unsigned int MD5_STR_SIZE = 32;
    char md5ChecksumString[33];
    md5ChecksumString[MD5_STR_SIZE] = (char)0;

    char* md5sum = strstr(string, "md5sum");

    if (md5sum != NULL) {
        strncpy(md5ChecksumString, md5sum + 8, MD5_STR_SIZE);
        return sdsnew(md5ChecksumString);
    }

    md5sum = strstr(string, "MD5");

    if (md5sum != NULL) {
        strncpy(md5ChecksumString, md5sum + 4, MD5_STR_SIZE);
        return sdsnew(md5ChecksumString);
    }

    md5sum = strstr(string, "md5");

    if (md5sum != NULL) {
        strncpy(md5ChecksumString, md5sum + 4, MD5_STR_SIZE);
        return sdsnew(md5ChecksumString);
    }

    return NULL;
}

static void checkMD5ChecksumNotice(const char * event, irc_parser_result_t *result) {
    if (!str_equals(event, "NOTICE")) {
        return;
    }

    if (result->num_params != 2) {
        return;
    }

    sds md5ChecksumSDS = extractMD5(result->params[1]);

    if (md5ChecksumSDS == NULL) {
        return;
    }

    if (lastDownload == NULL) {
        if (cfg.numDownloads == 1) {
            cfg.dccDownloadArray[0]->md5 = md5ChecksumSDS;
        }
        return;
    }

    startChecksumThread(md5ChecksumSDS, sdsdup(lastDownload->completePath));
}


void dump_event (irc_session_t * session, const char * event, irc_parser_result_t *result)
{
    sds param_string = sdsempty();
    unsigned int cnt;

    for (cnt = 0; cnt < result->num_params; cnt++) {
        if (cnt)
            param_string = sdscat(param_string, "|");

        char *message_without_color_codes = irc_color_strip_from_mirc(result->params[cnt]);
        param_string = sdscat(param_string, message_without_color_codes);
        free(message_without_color_codes);
    }

    logprintf(LOG_INFO, "Event \"%s\", origin: \"%s\", params: %d [%s]", event, result->nick ? result->nick : "NULL", cnt, param_string);
    sdsfree(param_string);
}

static void join_channels(irc_session_t *session) {
    for (uint32_t i = 0; i < cfg.numChannels; i++) {
        logprintf(LOG_INFO, "joining %s\n", cfg.channelsToJoin[i]);
        irc_cmd_join (session, cfg.channelsToJoin[i], 0);
    }
}

static void send_xdcc_requests(irc_session_t *session) {
    int i;
    if (!cfg_get_bit(&cfg, SENDED_FLAG)) {
        for (i = 0; cfg.dccDownloadArray[i] != NULL; i++) {
            char *botNick = cfg.dccDownloadArray[i]->botNick;
            char *xdccCommand = cfg.dccDownloadArray[i]->xdccCmd;

            logprintf(LOG_INFO, "/msg %s %s\n", botNick, xdccCommand);
            bool cmdSendingFailed = irc_cmd_msg(session, botNick, xdccCommand) == 1;

            if (cmdSendingFailed) {
                logprintf(LOG_ERR, "Cannot send xdcc command to bot!");
            }
        }

        cfg_set_bit(&cfg, SENDED_FLAG);
    }
}

static inline bool isPasswordAccepted(const char *message) {
    const char *password_sequences[] = {
        "Password accepted",
        "You are now identified",
        "I recognize you"
    };

    size_t num_sequences = sizeof(password_sequences) / sizeof(const char*);
    for (size_t i = 0; i < num_sequences; i++) {
        char *t = strstr(message, password_sequences[i]);
        if (t != NULL) {
            return true;
        }
    }

    return false;
}

void event_notice(irc_session_t * session, const char * event, irc_parser_result_t *result) {
    dump_event(session, event, result);
    checkMD5ChecksumNotice(event, result);
}

void event_mode(irc_session_t * session, const char * event, irc_parser_result_t *result) {
    if (cfg.login_command != NULL && result->num_params > 1) {
        if (str_equals(result->params[1], "+v")) {
            send_xdcc_requests(session);
        }
    }

}

void event_umode(irc_session_t * session, const char * event, irc_parser_result_t *result) {
    if (cfg.login_command != NULL) {
        if (str_equals(result->params[0], "+r")) {
            join_channels(session);
        }
    }
}


void event_join (irc_session_t * session, const char * event, irc_parser_result_t *result)
{
    irc_cmd_user_mode (session, "+i");

    if (cfg.sendDelay == NULL) {
        if (cfg.login_command == NULL) {
            send_xdcc_requests(session);
        }
    }
}

static void send_login_command(irc_session_t *session) {
    cfg.login_command = sdstrim(cfg.login_command, " \t");

    if (sdslen(cfg.login_command) >= 9) {
        sds user = sdsdup(cfg.login_command);
        sds auth_command = sdsdup(cfg.login_command);
        sdsrange(user, 0, 8);
        sdsrange(auth_command, 9, sdslen(auth_command));

        logprintf(LOG_INFO, "sending login-command: %s", cfg.login_command);

        bool cmdSendingFailed = irc_cmd_msg(session, user, auth_command) == 1;

        if (cmdSendingFailed) {
            logprintf(LOG_ERR, "Cannot send command to authenticate!");
        }

        sdsfree(user);
        sdsfree(auth_command);
    } else {
        logprintf(LOG_ERR, "the login-command is too short. cant send this login-command.");
    }
}


void event_connect (irc_session_t *session, const char * event, irc_parser_result_t *result)
{
    dump_event (session, event, result);

#ifdef ENABLE_SSL
    logprintf(LOG_INFO, "using cipher suite: %s", irc_get_ssl_ciphers_used(session));
#endif

    if (cfg.login_command != NULL) {
        send_login_command(session);
    }
    else {
        join_channels(session);
    }
}


void event_privmsg (irc_session_t * session, const char * event, irc_parser_result_t *result)
{

    dump_event (session, event, result);

    printf ("'%s' said me (%s): %s\n", 
            result->nick ? result->nick  : "someone",
            result->params[0], result->params[1] );
}

void event_numeric (irc_session_t * session, unsigned int event, irc_parser_result_t *result)
{
    char buf[24];
    snprintf (buf, sizeof(buf), "%d", event);

    dump_event (session, buf, result);
}

static bool isValidRequestFromNick(irc_session_t *session, const char *botNick) {
    unsigned int i = 0;
    
    struct dccDownload **dccDownloads = cfg.dccDownloadArray;

    if (dccDownloads == NULL) {
        return false;
    }

    if (cfg_get_bit(&cfg, ACCEPT_ALL_NICKS_FLAG)) {
        return true;
    }

    for (i = 0; dccDownloads[i]; i++) {
        if (strcasecmp(dccDownloads[i]->botNick, botNick) == 0) {
            return true;
        }
    }

    return false;
}

// This callback is used when we receive a file from the remote party

void callback_dcc_recv_file(irc_session_t * session, irc_dcc_t id, int status, void * ctx, const char * data, irc_dcc_size_t length) {
    if (data == NULL) {
        DBG_WARN("callback_dcc_recv_file called with data = NULL!");
        return;
    }

    if (ctx == NULL) {
        DBG_WARN("callback_dcc_recv_file called with ctx = NULL!");
        return;
    }

    if (length == 0) {
        DBG_WARN("callback_dcc_recv_file called with length = 0!");
        return;
    }

    if (status) {
        DBG_ERR("File sent error: %d\nerror desc: %s", status, irc_strerror(status));
        return;
    }

    struct dccDownloadContext *context = (struct dccDownloadContext*) ctx;
    struct dccDownloadProgress *progress = context->progress;

    progress->sizeRcvd += length;
    Write(context->fd, data, length);

    if (unlikely(progress->sizeRcvd == progress->completeFileSize)) {
        enableAlarm(0);
        outputProgress(progress);
        lastDownload = curDownload;
        printf("\nDownload completed!\n");
        fflush(NULL);

        Close(context->fd);
        context->fd = NULL;

        finishedDownloads++;

        if (!(cfg_get_bit(&cfg, VERIFY_CHECKSUM_FLAG))) {
            if (finishedDownloads == numActiveDownloads) {
                irc_cmd_quit(cfg.session, "Goodbye!");
            }
        }
        else {
            if (cfg.numDownloads == 1 && cfg.dccDownloadArray[0]->md5) {
                startChecksumThread(cfg.dccDownloadArray[0]->md5, sdsdup(lastDownload->completePath));
            }
        }
    }
}

void callback_dcc_resume_file_reverse (irc_session_t * session, irc_dcc_t dccid, int status, void * ctx, const char * data, irc_dcc_size_t length, const char * nick, const char *filename, unsigned long token) {
    struct dccDownloadContext *context = (struct dccDownloadContext*) ctx;

    DBG_OK("got to callback_dcc_resume_file_reverse\n");
    Seek(context->fd, length, SEEK_SET);
    DBG_OK("before irc_dcc_accept_reverse!\n");

    struct dccDownloadProgress *tdp = context->progress;
    tdp->sizeRcvd = length;
    tdp->sizeNow = length;
    tdp->sizeLast = length;

    int ret = irc_dcc_accept_reverse (session, dccid, ctx, callback_dcc_recv_file, nick, filename, tdp->completeFileSize, token);

    if (ret != 0) {
        logprintf(LOG_ERR, "Could wait for bot\nError was: %s\n", irc_strerror(irc_errno(cfg.session)));
        exitPgm(EXIT_FAILURE);
    }

    DBG_OK("after irc_dcc_accept_reverse!\n");
}

void callback_dcc_resume_file (irc_session_t * session, irc_dcc_t dccid, int status, void * ctx, const char * data, irc_dcc_size_t length) {
    struct dccDownloadContext *context = (struct dccDownloadContext*) ctx;

    DBG_OK("got to callback_dcc_resume_file\n");
    Seek(context->fd, length, SEEK_SET);
    DBG_OK("before irc_dcc_accept!\n");

    struct dccDownloadProgress *tdp = context->progress;
    tdp->sizeRcvd = length;
    tdp->sizeNow = length;
    tdp->sizeLast = length;

    int ret = irc_dcc_accept (session, dccid, ctx, callback_dcc_recv_file);

    if (ret != 0) {
        logprintf(LOG_ERR, "Could not connect to bot\nError was: %s\n", irc_strerror(irc_errno(cfg.session)));
        exitPgm(EXIT_FAILURE);
    }

    DBG_OK("after irc_dcc_accept!\n");
}

sds getAbsolutePath() {
    sds lastCharOfTargetDir = sdsdup(cfg.targetDir);
    sdsrange(lastCharOfTargetDir, -1, -1);
    sds absolutePath = sdsempty();
    
    if (!str_equals(lastCharOfTargetDir, getPathSeperator())) {
        DBG_OK("last char of dir was: %s", lastCharOfTargetDir);
        absolutePath = sdscatprintf(absolutePath, "%s%s", cfg.targetDir, getPathSeperator());
    }
    else {
        absolutePath = sdscatprintf(absolutePath, "%s", cfg.targetDir);
    }
    
    sdsfree(lastCharOfTargetDir);
    return absolutePath;
}

sds getCompletePath(const char *filename) {
    sds fileName = sdsnew(filename);
    sds absolutePath = getAbsolutePath();
    sds completePath = sdsempty();
    completePath = sdscatprintf(completePath, "%s%s", absolutePath, fileName);
    sdsfree(fileName);
    sdsfree(absolutePath);
    return completePath;
}

bool containsIllegalChars(const char *filename) {
    /* chars / and \ are not permitted to appear in a valid filename. if someone wants to send us such a file 
       then something is definately wrong. so just exit pgm then and print error msg to user.*/
    char *illegalFilenameChars = "/\\";
    sds fileName = sdsnew(filename);
    bool ret = false;
    
    if (sdscontains(fileName, illegalFilenameChars, strlen(illegalFilenameChars))) {
        ret = true;
    }
    
    sdsfree(fileName);
    return ret;
}

struct dccDownloadContext* prepareRecvFileRequest (irc_session_t *session, const char *nick, const char *addr, const char *filename, irc_dcc_size_t size, irc_dcc_t dccid) {
    DBG_OK("DCC send [%d] requested from '%s' (%s): %s (%" IRC_DCC_SIZE_T_FORMAT " bytes)\n", dccid, nick, addr, filename, size);

    if (containsIllegalChars(filename)) {
        /* filename contained bad chars. print msg and exit...*/
        logprintf(LOG_ERR, "Someone wants to send us a file that contains / or \\. This is not permitted.\nFilename was: %s", filename);
        exitPgm(EXIT_FAILURE);
    }

    sds absolutePath = getAbsolutePath();

    if (!dir_exists(absolutePath)) {
        logprintf(LOG_INFO, "Creating following folder to store downloads: %s", absolutePath);
        if (mkdir(absolutePath, 0755) == -1) {
            DBG_WARN("cant create dir %s", absolutePath);
            perror("mkdir");
        }
    }

    sds completePath = getCompletePath(filename);
    sdsfree(absolutePath);

    struct dccDownloadProgress *progress = newDccProgress(completePath, size);
    curDownload = progress;

    struct dccDownloadContext *context = Malloc(sizeof(struct dccDownloadContext));
    downloadContext[numActiveDownloads] = context;
    numActiveDownloads++;
    context->progress = progress;

    DBG_OK("nick at recvFileReq is %s\n", nick);
    return context;
}


void recvFileRequestReverse (irc_session_t *session, const char *nick, const char *addr, const char *filename, irc_dcc_size_t size, irc_dcc_t dccid, unsigned long token) {
    struct dccDownloadContext *context = prepareRecvFileRequest(session, nick, addr, filename, size, dccid);
    sds completePath = getCompletePath(filename);

    if(file_exists (completePath)) {
        context->fd = Open(completePath, "a");

        off_t fileSize = get_file_size(completePath);

        if (size == (irc_dcc_size_t) fileSize) {
            logprintf(LOG_ERR, "file %s is already downloaded, exit pgm now.", completePath);
            exitPgm(EXIT_FAILURE);
        }

        /* file already exists but is empty. so accept it, rather than resume... */
        if (fileSize == 0) {
            goto accept_flag_reverse;
        }

        logprintf(LOG_INFO, "file %s already exists, need to resume.\n", completePath);
        irc_dcc_resume_reverse(session, dccid, context, callback_dcc_resume_file_reverse, nick, filename, fileSize, token);
    } else {
        int ret;
        context->fd = Open(completePath, "w");
        logprintf(LOG_INFO, "file %s does not exist. creating file and waiting for connection from bot.", completePath);
accept_flag_reverse:
        ret = irc_dcc_accept_reverse(session, dccid, context, callback_dcc_recv_file, nick, filename, size, token);
        if (ret != 0) {
            logprintf(LOG_ERR, "Could not wait for connection from bot\nError was: %s\n", irc_strerror(irc_errno(cfg.session)));
            exitPgm(EXIT_FAILURE);
        }
    }
    sdsfree(completePath);
}


void recvFileRequest (irc_session_t *session, const char *nick, const char *addr, const char *filename, irc_dcc_size_t size, irc_dcc_t dccid)
{
    struct dccDownloadContext *context = prepareRecvFileRequest(session, nick, addr, filename, size, dccid);
    sds completePath = getCompletePath(filename);
    if(file_exists (completePath)) {
        context->fd = Open(completePath, "a");

        off_t fileSize = get_file_size(completePath);

        if (size == (irc_dcc_size_t) fileSize) {
            logprintf(LOG_ERR, "file %s is already downloaded, exit pgm now.", completePath);
            exitPgm(EXIT_FAILURE);
        }

        /* file already exists but is empty. so accept it, rather than resume... */
        if (fileSize == 0) {
            goto accept_flag;
        }

        logprintf(LOG_INFO, "file %s already exists, need to resume.\n", completePath);
        irc_dcc_resume(session, dccid, context, callback_dcc_resume_file, nick, fileSize);
    }
    else {
        int ret;
        context->fd = Open(completePath, "w");
        logprintf(LOG_INFO, "file %s does not exist. creating file and downloading it now.", completePath);
accept_flag:
        ret = irc_dcc_accept(session, dccid, context, callback_dcc_recv_file);
        if (ret != 0) {
            logprintf(LOG_ERR, "Could not connect to bot\nError was: %s\n", irc_strerror(irc_errno(cfg.session)));
            exitPgm(EXIT_FAILURE);
        }
    }

    sdsfree(completePath);
}

irc_dcc_size_t getCurrentTransferSpeed() {
    irc_dcc_size_t speed = 0;

    for (uint32_t i = 0; i < numActiveDownloads; i++) {
        speed += downloadContext[i]->progress->averageSpeed;
    }

    return speed;
}

static inline irc_dcc_size_t getAdjustmentValue (irc_dcc_size_t a, irc_dcc_size_t b) {
	return (b != 0) ? a / b : 1;
}

sleeptime_t getSleepTime() {
    const int factor = 5;
    static sleeptime_t sleepTime = 0;
    irc_dcc_size_t currentSpeed = getCurrentTransferSpeed();

    if (cfg.maxTransferSpeed == NO_SPEED_LIMIT) {
        return 0;
    }

    if (currentSpeed > cfg.maxTransferSpeed) {
        sleepTime += getAdjustmentValue(currentSpeed, cfg.maxTransferSpeed) * factor;
    } else {
        irc_dcc_size_t adjustment = getAdjustmentValue(cfg.maxTransferSpeed, currentSpeed) * factor;
        if (sleepTime >= adjustment) {
            sleepTime -= adjustment;
        }
        else {
            sleepTime = 0;
        }
    }

    return sleepTime;
}

void sleeping() {
    sleeptime_t sleepTime = getSleepTime();
    if (sleepTime != 0) {
        int ret = usleep(sleepTime);
        if (ret != 0) {
            if (errno != EINTR) {	
                logprintf(LOG_ERR, "Problem while usleep!\nError was: %s\n", strerror(errno));
                exitPgm(EXIT_FAILURE);
            }
        } 
    }
}

bool shouldSendXdccRequests(irc_session_t* session) {
    if (cfg.sendDelay != NULL && !cfg_get_bit(&cfg, SENDED_FLAG)) {
        if (cfg.sendDelay->timeToSendCommand < time(NULL)) {
            return true;
        }
    }

    return false;
}

void print_output_callback (irc_session_t *session) {
    if (unlikely(shouldSendXdccRequests(session))) {
        send_xdcc_requests(session);
    }

    if (unlikely(cfg_get_bit(getCfg(), OUTPUT_FLAG))) {
        output_all_progesses();
        cfg_clear_bit(getCfg(), OUTPUT_FLAG);
    }

    sleeping();
}

void initCallbacks(irc_callbacks_t *callbacks) {
    memset (callbacks, 0, sizeof(*callbacks));

    callbacks->event_connect = event_connect;
    callbacks->event_join = event_join;
    callbacks->event_dcc_send_req = recvFileRequest;
    callbacks->event_dcc_send_req_reverse = recvFileRequestReverse;
    callbacks->event_ctcp_rep = dump_event;
    callbacks->event_ctcp_action = dump_event;
    callbacks->event_unknown = dump_event;
    callbacks->event_privmsg = dump_event;
    callbacks->event_notice = event_notice;
    callbacks->event_umode = event_umode;
    callbacks->event_mode = event_mode;
    callbacks->keep_alive_callback = print_output_callback;
}

int main (int argc, char **argv)
{
    int ret = -1;
    initRand();
#ifdef ENABLE_ANSI_COLORS
    enableAnsiColorCodes();
#endif
    memset(&cfg, 0, sizeof(struct xdccGetConfig));

    cfg.logLevel = LOG_WARN;
    cfg.port = 6667;
    cfg.maxTransferSpeed = NO_SPEED_LIMIT;
//    cfg.maxTransferSpeed = getSizeOf(1, "MByte");

    const char *homeDir = getHomeDir();
    sds targetDir = sdscatprintf(sdsempty(), "%s%s%s", homeDir, getPathSeperator(), "Downloads");

#ifdef WINDOWS_BUILD
    free((void*)homeDir);
#endif

    cfg.targetDir = targetDir;

    parseConfigFile(&cfg);
    parseArguments(argc, argv, &cfg);

    cfg.ircServer = cfg.args[0];

    cfg.channelsToJoin = parseChannels(cfg.args[1], &cfg.numChannels);
    cfg.dccDownloadArray = parseDccDownloads(cfg.args[2], &cfg.numDownloads);

    downloadContext = Calloc(cfg.numDownloads, sizeof(struct downloadContext*));

    createInterruptHandler(interrupt_handler);
    createAlarmHandler(output_handler);

    irc_callbacks_t callbacks;
    initCallbacks(&callbacks);
    cfg.session = irc_create_session (&callbacks);

    if (!cfg.session) {
        logprintf(LOG_ERR, "Could not create session\n");
        exitPgm(EXIT_FAILURE);
    }

    logprintf(LOG_INFO, "test message for info");
    logprintf(LOG_QUIET, "test message for quiet");
    logprintf(LOG_WARN, "test message for warn");
    logprintf(LOG_ERR, "test message for error");

    if (cfg.nick == NULL) {
        cfg.nick = sdsnewlen(NULL, NICKLEN);
        createRandomNick(NICKLEN, cfg.nick);
    }

    logprintf(LOG_INFO, "nick is %s\n", cfg.nick);
    
    irc_set_verify_nick_callback(cfg.session, isValidRequestFromNick);

#ifdef ENABLE_SSL
    irc_set_cert_verify_callback(cfg.session, openssl_check_certificate_callback);
#endif

    if (cfg_get_bit(&cfg, USE_IPV4_FLAG)) {
        ret = irc_connect4(cfg.session, cfg.ircServer, cfg.port, 0, cfg.nick, 0, 0);
    }
#ifdef ENABLE_IPV6
    else if (cfg_get_bit(&cfg, USE_IPV6_FLAG)) {
        ret = irc_connect6(cfg.session, cfg.ircServer, cfg.port, 0, cfg.nick, 0, 0);
    }
#endif
    else {
        ret = irc_connect(cfg.session, cfg.ircServer, cfg.port, 0, cfg.nick, 0, 0);
    }

    if (ret != 0) {
        logprintf(LOG_ERR, "Could not connect to server %s and port %u.\nError was: %s\n", cfg.ircServer, cfg.port, irc_strerror(irc_errno(cfg.session)));
        exitPgm(EXIT_FAILURE);
    }

    enableAlarm(1);

    ret = irc_run (cfg.session);

    if (ret != 0) {
        if (irc_errno(cfg.session) != LIBIRC_ERR_TERMINATED && irc_errno(cfg.session) != LIBIRC_ERR_CLOSED) {
            logprintf(LOG_ERR, "Could not connect or I/O error at server %s and port %u\nError was:%s\n", cfg.ircServer, cfg.port, irc_strerror(irc_errno(cfg.session)));
            exitPgm(EXIT_FAILURE);
        }
    }

    doCleanUp();
    return EXIT_SUCCESS;
}
