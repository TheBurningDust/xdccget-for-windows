#include <stdlib.h>

#ifndef _MSC_VER
#include <argp.h>
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include "getopt.h"
#endif

#include "helper.h"
#include "config.h"
#include "os_specific.h"

const char *argp_program_version = "xdccget 1.8";
const char *argp_program_bug_address ="<nobody@nobody.org>";

/* Program documentation. */
static char doc[] =
"xdccget -- download from cmd with xdcc";

/* A description of the arguments we accept. */
static char args_doc[] = "<server> <channel(s)> <bot cmds>";

#define OPT_ACCEPT_ALL_NICKS 1
#define OPT_DONT_CONFIRM_OFFSETS 2
#define OPT_THROTTLE_DOWNLOAD 3
#define OPT_DELAY_COMMAND 4
#define OPT_LISTEN_IP_COMMAND 5
#define OPT_LISTEN_PORT_COMMAND 6
#define OPT_ACCEPT_ALL_CERTS 7

static void set_quiet_loglevel(struct xdccGetConfig* cfg) {
    DBG_OK("setting log-level as quiet.");
    cfg->logLevel = LOG_QUIET;
}

static void set_warn_loglevel(struct xdccGetConfig* cfg) {
    DBG_OK("setting log-level as warn.");
    cfg->logLevel = LOG_WARN;
}

static void set_info_loglevel(struct xdccGetConfig* cfg) {
    DBG_OK("setting log-level as info.");
    cfg->logLevel = LOG_INFO;
}

static void set_verify_checksum(struct xdccGetConfig* cfg) {
    DBG_OK("setting verify checksum option.");
    cfg_set_bit(cfg, VERIFY_CHECKSUM_FLAG);
}

static void set_target_dir(struct xdccGetConfig* cfg, char* arg) {
    DBG_OK("setting target dir as %s", arg);
    cfg->targetDir = sdsnew(arg);
}

static void set_nickname(struct xdccGetConfig* cfg, char* arg) {
    DBG_OK("setting nickname as %s", arg);
    cfg->nick = sdsnew(arg);
}

static void set_login_command(struct xdccGetConfig* cfg, char* arg) {
    DBG_OK("setting login-command as %s", arg);
    cfg->login_command = sdsnew(arg);
}

static void set_port(struct xdccGetConfig* cfg, char* arg) {
    cfg->port = (unsigned short)strtoul(arg, NULL, 0);
    DBG_OK("setting port as %u", cfg->port);
}

static void set_accept_all_nicks(struct xdccGetConfig* cfg) {
    DBG_OK("setting accept all nicks.");
    cfg_set_bit(cfg, ACCEPT_ALL_NICKS_FLAG);
}

static void set_dont_confirm_offsets(struct xdccGetConfig* cfg) {
    DBG_OK("setting dont confirm offsets.");
    cfg_set_bit(cfg, DONT_CONFIRM_OFFSETS_FLAG);
}

static void set_throttle_download(struct xdccGetConfig* cfg, char* arg) {
    DBG_OK("setting throttle download to %s.", arg);
    sds val = sdsnew(arg);
    val = sdstrim(val, " \t");
    setMaxTransferSpeed(cfg, val);
    sdsfree(val);
}

static void set_delay_command(struct xdccGetConfig* cfg, char* arg) {
    sds val = sdsnew(arg);
    val = sdstrim(val, " \t");
    setDelay(cfg, val);
    sdsfree(val);
}

static void set_listen_ip(struct xdccGetConfig* cfg, char* arg) {
    struct in_addr addr_buf;
    
    memset(&addr_buf, 0, sizeof(addr_buf));

    if (inet_pton(AF_INET, arg, &addr_buf) == 0) {
        logprintf(LOG_ERR, "the listen ip %s is not valid ipv4 address.", arg);
        exitPgm(-1);
    }
    
    sds val = sdsnew(arg);
    val = sdstrim(val, " \t");
    cfg->listen_ip = val;
}

static void set_accept_all_certs(struct xdccGetConfig* cfg) {
    cfg_set_bit(cfg, ALLOW_ALL_CERTS_FLAG);
}

static void set_listen_port(struct xdccGetConfig* cfg, char* arg) {
    cfg->listen_port = (unsigned short)strtoul(arg, NULL, 0);
}

static void set_use_ipv4(struct xdccGetConfig* cfg) {
    DBG_OK("using ipv4 only now");
    cfg_set_bit(cfg, USE_IPV4_FLAG);
}

static void set_use_ipv6(struct xdccGetConfig* cfg) {
    DBG_OK("using ipv6 only now");
    cfg_set_bit(cfg, USE_IPV6_FLAG);
}

#ifdef _MSC_VER
static int show_version_info_called = 0;

static void show_version_info(struct xdccGetConfig* cfg) {
    show_version_info_called = 1;
    printf("%s\n", argp_program_version);
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#if defined(_MSC_FULL_VER)
#define MSVC_VERSION_STR STR(_MSC_FULL_VER)
#elif defined(_MSC_VER)
#define MSVC_VERSION_STR STR(_MSC_VER)
#else
#define MSVC_VERSION_STR "unknown"
#endif
    printf("compiled with Visual Studio version %s and %s\n", MSVC_VERSION_STR, OPENSSL_VERSION_TEXT);
}
#endif

#ifndef _MSC_VER
/* The options we understand. */
static struct argp_option options[] = {
{"verbose",  'v', 0,      0,  "Produce verbose output", 0 },
{"quiet",    'q', 0,      0,  "Don't produce any output", 0 },
{"information",    'i', 0,      0,  "Produce information output.", 0 },
{"checksum-verify", 'c', 0,      0,  "Stay connected after download completed to verify checksums.", 0 },
{"ipv4",   '4', 0,      0,  "Use ipv4 to connect to irc server.", 0 },
#ifdef ENABLE_IPV6
{"ipv6",   '6', 0,      0,  "Use ipv6 to connect to irc server.", 0 },
#endif
{"port",   'p', "<port number>",      0,  "Use the following port to connect to server. default is 6667.", 0 },
{"directory",   'd', "<download-directory>",      0,  "Directory, where to place the files." , 0 },
{"nick",   'n', "<nickname>",      0,  "Use this specific nickname while connecting to the irc-server.", 0 },
{"login",   'l', "<login-command>",      0,  "Use this login-command to authorize your nick to the irc-server after connecting.", 0 },
{"accept-all-nicks",   OPT_ACCEPT_ALL_NICKS, 0,      0,  "Accept DCC send requests from ALL bots and do not verify any nicknames of incoming dcc requests.", 0 },
{"accept-all-certs",   OPT_ACCEPT_ALL_CERTS, 0,      0,  "Accept all certificates in tls handshakes, ignore all errors and do not abort the handshake on any tls related error.", 0 },
{"dont-confirm-offsets",   OPT_DONT_CONFIRM_OFFSETS, 0,      0,  "Do not send file offsets to the bots. Can be used on bots where the transfer gets stucked after a short while.", 0 },
{"throttle",  OPT_THROTTLE_DOWNLOAD, "<speed>",      0,  "Limit the maximum transfer speed for the downloads in each xdccget instance to the specified value per seconds. valid suffixes are KByte, MByte and TByte - e.g. 1Mbyte throttles the speed to 1MByte/s.", 0 },
{"delay", OPT_DELAY_COMMAND, "<time in seconds>",      0,  "Delay the sending of the xdcc send ccommand to specified seconds.", 0 },
{"listen-ip", OPT_LISTEN_IP_COMMAND, "<ipv4 address>",      0,  "When using passive dcc use this listen ip address (normally your external ip address).", 0 },
{"listen-port", OPT_LISTEN_PORT_COMMAND, "<port number>",      0,  "When using passive dcc use this listen port (needs to enabled in your router).", 0 },
{ 0 }
};

static error_t parse_opt(int key, char* arg, struct argp_state* state);

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc, NULL, NULL, NULL };

/* Parse a single option. */
static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    /* Get the input argument from argp_parse, which we
      know is a pointer to our arguments structure. */
    struct xdccGetConfig* cfg = state->input;

    switch (key) {
    case 'q':
        set_quiet_loglevel(cfg);
        break;

    case 'v':
        set_warn_loglevel(cfg);
        break;

    case 'i':
        set_info_loglevel(cfg);
        break;

    case 'c':
        set_verify_checksum(cfg);
        break;

    case 'd':
        set_target_dir(cfg, arg);
        break;

    case 'n':
        set_nickname(cfg, arg);
        break;

    case 'l':
        set_login_command(cfg, arg);
        break;

    case 'p':
        set_port(cfg, arg);
        break;

    case OPT_ACCEPT_ALL_NICKS:
        set_accept_all_nicks(cfg);
        break;
    case OPT_ACCEPT_ALL_CERTS:
        set_accept_all_certs(cfg);
        break;
    case OPT_DONT_CONFIRM_OFFSETS:
        set_dont_confirm_offsets(cfg);
        break;
    case OPT_THROTTLE_DOWNLOAD:
        set_throttle_download(cfg, arg);
        break;
    case OPT_DELAY_COMMAND:
        set_delay_command(cfg, arg); 
        break;
    case OPT_LISTEN_IP_COMMAND:
        set_listen_ip(cfg, arg);
        break;
    case OPT_LISTEN_PORT_COMMAND:
        set_listen_port(cfg, arg);
        break;
    case '4':
        set_use_ipv4(cfg);
        break;

#ifdef ENABLE_IPV6
    case '6':
        set_use_ipv6(cfg);
        break;
#endif

    case ARGP_KEY_ARG:
    {
        if (state->arg_num >= 3)
            /* Too many arguments. */
            argp_usage(state);

        cfg->args[state->arg_num] = arg;
    }
    break;

    case ARGP_KEY_END:
        if (state->arg_num < 3)
            /* Not enough arguments. */
            argp_usage(state);
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
#else

static struct option long_options[] = {
    /*   NAME       ARGUMENT           FLAG  SHORTNAME */
        {"verbose",     no_argument, NULL, 'v'},
        {"quiet",  no_argument,       NULL, 'q'},
        {"information",  no_argument, NULL, 'i'},
        {"checksum-verify", no_argument,       NULL, 0},
        {"ipv4",  no_argument, NULL, '4'},
#ifdef ENABLE_IPV6
        {"ipv6",   no_argument, NULL, '6'},
#endif
        {"port",  required_argument, NULL, 'p'},
        {"directory",  required_argument, NULL, 'd'},
        {"nick",  required_argument, NULL, 'n'},
        {"login",  required_argument, NULL, 'l'},
        {"accept-all-nicks",  no_argument, NULL, 0},
        {"accept-all-certs",  no_argument, NULL, 0},
        {"dont-confirm-offsets",  no_argument, NULL, 0},
        {"throttle",  required_argument, NULL, 0},
        {"delay",  required_argument, NULL, 0},
        {"listen-ip",  required_argument, NULL, 0},
        {"listen-port",  required_argument, NULL, 0},
        {"version",  no_argument, NULL, 0},
        {NULL,      0,                 NULL, 0}
};


static void eval_long_option(struct xdccGetConfig* cfg, char* option_name, char* optarg) {
    if (strcmp(option_name, "checksum-verify") == 0) {
        set_verify_checksum(cfg);
    }
    else if (strcmp(option_name, "accept-all-nicks") == 0) {
        set_accept_all_nicks(cfg);
    }
    else if (strcmp(option_name, "accept-all-certs") == 0) {
        set_accept_all_certs(cfg);
    }
    else if (strcmp(option_name, "dont-confirm-offsets") == 0) {
        set_dont_confirm_offsets(cfg);
    }
    else if (strcmp(option_name, "throttle") == 0) {
        set_throttle_download(cfg, optarg);
    }
    else if (strcmp(option_name, "delay") == 0) {
        set_delay_command(cfg, optarg);
    }
    else if (strcmp(option_name, "listen-ip") == 0) {
        set_listen_ip(cfg, optarg);
    }
    else if (strcmp(option_name, "listen-port") == 0) {
        set_listen_port(cfg, optarg);
    }
    else if (strcmp(option_name, "version") == 0) {
        show_version_info(cfg);
    }
    else {
        DBG_ERR("invalid argument selection %s", option_name);
    }
}

static void print_usage_message() {
    if (shouldColorOutput()) {
        printf("Usage: xdccget.exe %s<optional options>%s %s<server>%s %s<channel(s)>%s %s<bot cmds>%s\n", KCYN, KNRM, KGRN, KNRM, KGRN, KNRM, KGRN, KNRM);
        printf("For example: xdccget.exe %s--port=6667%s %s\"irc.sample.net\"%s %s\"#sample-channel\"%s %s\"sample-xdccget-bot xdcc send #42\"%s\n\n", KCYN, KNRM, KGRN, KNRM, KGRN, KNRM, KGRN, KNRM);
    }
    else {
        printf("Usage: xdccget.exe <optional options> <server> <channel(s)> <bot cmds>\n");
        printf("For example: xdccget.exe --port=6667 \"irc.sample.net\" \"#sample-channel\" \"sample-xdccget-bot xdcc send #42\"\n\n");
    }
    printf("The supported optional options are:\n");
    int num_options = sizeof(long_options) / sizeof(struct option);

    for (int i = 0; i < num_options-1; i++) {
        struct option long_option = long_options[i];
        if (long_option.has_arg) {
            printf("--%s=<%s>", long_option.name, long_option.name);
        }
        else {
            printf("--%s", long_option.name);
        }
        printf("\n");
    }

    fflush(stdout);
}
#endif

void parseArguments(int argc, char** argv, struct xdccGetConfig* cfg) {
    int ret = 0;

#ifndef _MSC_VER
    /* Parse our arguments; every option seen by parse_opt will
      be reflected in arguments. */
    ret = argp_parse(&argp, argc, argv, 0, 0, cfg);
#else
    int option_index = 0;
    int c;

    while ((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != -1) {
        int this_option_optind = optind ? optind : 1;
        switch (c) {
        case 0:
            eval_long_option(cfg, long_options[option_index].name, optarg);
            break;
        case 'q':
            set_quiet_loglevel(cfg);
            break;

        case 'v':
            set_warn_loglevel(cfg);
            break;

        case 'i':
            set_info_loglevel(cfg);
            break;

        case 'c':
            set_verify_checksum(cfg);
            break;

        case 'd':
            set_target_dir(cfg, optarg);
            break;

        case 'n':
            set_nickname(cfg, optarg);
            break;

        case 'l':
            set_login_command(cfg, optarg);
            break;

        case 'p':
            set_port(cfg, optarg);
            break;
        case '4':
            set_use_ipv4(cfg);
            break;

#ifdef ENABLE_IPV6
        case '6':
            set_use_ipv6(cfg);
            break;
#endif
        default:
            logprintf(LOG_INFO, "?? getopt returned character code 0%o ??\n", c);
        }
    }

    int actual_argument_counter = 0;

    for (int i = optind; i < argc; i++)
    {
        actual_argument_counter++;
    }

    if (!show_version_info_called) {
        if (actual_argument_counter != 3) {
            print_usage_message();
            exitPgm(0);
        }
    }
    else {
        exitPgm(0);
    }

    cfg->args[0] = argv[optind];
    cfg->args[1] = argv[optind+1];
    cfg->args[2] = argv[optind+2];

#endif

	if (ret != 0) {
		logprintf(LOG_ERR, "the parsing of the command line options failed");
	}
}

struct dccDownload* newDccDownload(sds botNick, sds xdccCmd) {
    struct dccDownload *t = (struct dccDownload*) Malloc(sizeof (struct dccDownload));
    t->botNick = botNick;
    t->xdccCmd = xdccCmd;
    t->md5 = NULL;
    return t;
}

void freeDccDownload(struct dccDownload *t) {
    sdsfree(t->botNick);
    sdsfree(t->xdccCmd);
    FREE(t);
}

struct dccDownloadProgress* newDccProgress(char *completePath, irc_dcc_size_t complFileSize) {
    struct dccDownloadProgress *t = (struct dccDownloadProgress*) Malloc(sizeof (struct dccDownloadProgress));
    t->completeFileSize = complFileSize;
    t->sizeRcvd = 0;
    t->sizeNow = 0;
    t->sizeLast = 0;
    t->completePath = completePath;
    t->curSpeed.speedIndex = 0;
    for (int i = 0; i < NUM_AVERAGE_SPEED_VALUES; i++) {
        t->curSpeed.curSpeed[i] = 0;
    }
    t->curSpeed.isSpeedArrayFilled = false;
    return t;

}

void freeDccProgress(struct dccDownloadProgress *progress) {
    sdsfree(progress->completePath);
    FREE(progress);
}

void parseDccDownload(char *dccDownloadString, sds *nick, sds *xdccCmd) {
    size_t i;
    size_t strLen = strlen(dccDownloadString);
    size_t spaceFound = 0;

    for (i = 0; i < strLen; i++) {
        if (dccDownloadString[i] == ' ') {
            spaceFound = i;
            break;
        }
    }

    size_t nickLen = spaceFound + 1;
    size_t cmdLen = (strLen - spaceFound) + 1;

    DBG_OK("nickLen = %zu, cmdLen = %zu", nickLen, cmdLen);

    sds nickPtr = sdsnewlen(NULL, nickLen);
    sds xdccPtr = sdsnewlen(NULL, cmdLen);

    nickPtr = sdscpylen(nickPtr, dccDownloadString, nickLen - 1);
    xdccPtr = sdscpylen(xdccPtr, dccDownloadString + (spaceFound + 1), cmdLen - 1);

    *nick = nickPtr;
    *xdccCmd = xdccPtr;
}

sds* parseChannels(char *channelString, uint32_t *numChannels) {
    DBG_OK("in parseChannels");
    int numFound = 0;
    char *seperator = ",";
    DBG_OK("sdssplitlen with =%s and %d", channelString, *numChannels);
    sds *splittedString = sdssplitlen(channelString, strlen(channelString), seperator, strlen(seperator), &numFound);
    if (splittedString == NULL) {
        DBG_ERR("splittedString = NULL, cant continue from here.");
        return NULL;
    }
    int i = 0;

    for (i = 0; i < numFound; i++) {
        sdstrim(splittedString[i], " \t");
        DBG_OK("%d: '%s'", i, splittedString[i]);
    }

    *numChannels = numFound;

    return splittedString;
}

struct dccDownload** parseDccDownloads(char *dccDownloadString, unsigned int *numDownloads) {
    int numFound = 0;
    int i = 0, j = 0;
    char *seperator = ",";

    sds *splittedString = sdssplitlen(dccDownloadString, strlen(dccDownloadString), seperator, strlen(seperator), &numFound);

    if (splittedString == NULL) {
        DBG_ERR("splittedString = NULL, cant continue from here.");
        return NULL;
    }

    struct dccDownload **dccDownloadArray = (struct dccDownload**) Calloc(numFound + 1, sizeof (struct dccDownload*));

    *numDownloads = numFound;

    for (i = 0; i < numFound; i++) {
        sdstrim(splittedString[i], " \t");
        sds nick = NULL;
        sds xdccCmd = NULL;
        DBG_OK("%d: '%s'\n", i, splittedString[i]);
        parseDccDownload(splittedString[i], &nick, &xdccCmd);
        DBG_OK("%d: '%s' '%s'\n", i, nick, xdccCmd);
        if (nick != NULL && xdccCmd != NULL) {
            dccDownloadArray[j] = newDccDownload(nick, xdccCmd);
            j++;
        }
        else {
            if (nick != NULL)
                sdsfree(nick);

            if (xdccCmd != NULL)
                sdsfree(xdccCmd);
        }
        sdsfree(splittedString[i]);
    }

    FREE(splittedString);
    return dccDownloadArray;
}
