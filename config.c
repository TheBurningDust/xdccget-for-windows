#include "config.h"

#include "file.h"
#include "helper.h"

#ifndef _MSC_VER
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock.h>
#endif

static void downloadDirCallback(struct xdccGetConfig *config, sds value);
static void parseLogLevel(struct xdccGetConfig *config, sds value);
static void allowAllCertsCallback(struct xdccGetConfig *config, sds value);
static void verifyChecksumsCallback (struct xdccGetConfig *config, sds value);
static void confirmFileOffsetsCallback (struct xdccGetConfig *config, sds value);
static void maxTransferSpeedCallback (struct xdccGetConfig *config, sds value);
static void listenIpCallback (struct xdccGetConfig *config, sds value);
static void listenPortCallback (struct xdccGetConfig *config, sds value);

typedef void (*ConfigLineParserFunction) (struct xdccGetConfig *config, sds value);

struct ConfigLineParser {
    char *type;
    ConfigLineParserFunction parse_line;
};

static struct ConfigLineParser configLineCallbacks[] = {
    {"downloadDir",     downloadDirCallback},
    {"logLevel",        parseLogLevel},
    {"allowAllCerts",   allowAllCertsCallback},
    {"verifyChecksums", verifyChecksumsCallback},
    {"confirmFileOffsets", confirmFileOffsetsCallback},
    {"maxTransferSpeed", maxTransferSpeedCallback},
    {"listenIp", listenIpCallback},
    {"listenPort", listenPortCallback},
};

static void verifyChecksumsCallback (struct xdccGetConfig *config, sds value) {
    if (str_equals(value, "true")) {
        cfg_set_bit(config, VERIFY_CHECKSUM_FLAG);
    }
    else {
        cfg_clear_bit(config, VERIFY_CHECKSUM_FLAG);
    }
}

static void allowAllCertsCallback(struct xdccGetConfig *config, sds value) {
     if (str_equals(value, "true")) {
        cfg_set_bit(config, ALLOW_ALL_CERTS_FLAG);
     }
     else {
        cfg_clear_bit(config, ALLOW_ALL_CERTS_FLAG);
     }
}

static void downloadDirCallback(struct xdccGetConfig *config, sds value) {
    sdsfree(config->targetDir);
    config->targetDir = sdsdup(value);
}

static void confirmFileOffsetsCallback (struct xdccGetConfig *config, sds value) {
     if (str_equals(value, "true")) {
        cfg_clear_bit(config, DONT_CONFIRM_OFFSETS_FLAG);
     }
     else {
        cfg_set_bit(config, DONT_CONFIRM_OFFSETS_FLAG);
     }
}

static void maxTransferSpeedCallback (struct xdccGetConfig *config, sds value) {
     setMaxTransferSpeed(config, value);
}

static void listenIpCallback (struct xdccGetConfig *config, sds value) {
    struct in_addr addr_buf;
    
    memset(&addr_buf, 0, sizeof(addr_buf));

    if (inet_pton(AF_INET, value, &addr_buf) == 0) {
        logprintf(LOG_ERR, "the listen ip %s in config file is not valid ipv4 address.", value);
        exitPgm(-1);
    }
    
    config->listen_ip = sdsdup(value);
}

static void listenPortCallback (struct xdccGetConfig *config, sds value) {
    config->listen_port = (unsigned short)strtoul(value, NULL, 0);
}

static void parseLogLevel(struct xdccGetConfig *config, sds logLevel) {
    if (str_equals(logLevel, "information")) {
        config->logLevel = LOG_INFO;
    }
    else if (str_equals(logLevel, "warn")) {
        config->logLevel = LOG_WARN;
    }
    else if (str_equals(logLevel, "error")) {
        config->logLevel = LOG_ERR;
    }
    else if (str_equals(logLevel, "quiet")) {
        config->logLevel = LOG_QUIET;
    }
}

static sds getDefaultConfigContent() {
    sds downloadDir = sdscatprintf(sdsempty(), "%s%s%s", getHomeDir(), getPathSeperator(), "Downloads");
    sds content = sdsempty();

    content = sdscatprintf(content, "# default directory where to store the downloads\n");
    content = sdscatprintf(content, "downloadDir=%s\n", downloadDir);
    content = sdscatprintf(content, "# default logging level, valid options: information, warn, error, quiet\n");
    content = sdscatprintf(content, "logLevel=information\n");
    content = sdscatprintf(content, "# allow all certificates and dont validate them\n");
    content = sdscatprintf(content, "allowAllCerts=true\n");
    content = sdscatprintf(content, "# stay connected after downloads finished to automatically verify checksums\n");
    content = sdscatprintf(content, "verifyChecksums=false\n");
    content = sdscatprintf(content, "# Do not send file offsets to the bots if set to false. Can be used on bots where the transfer gets stucked after a short while.\n");
    content = sdscatprintf(content, "confirmFileOffsets=false\n");
    content = sdscatprintf(content, "# Limit the maximum transfer speed for the downloads in each xdccget instance to the specified value per seconds. valid suffixes are KByte, MByte and TByte\n");
    content = sdscatprintf(content, "#maxTransferSpeed=1MByte\n");
    content = sdscatprintf(content, "# Sets the listen ip for passive dcc transfers. Should normally your external ip address.\n");
    content = sdscatprintf(content, "#listenIp=85.48.89.195\n");
    content = sdscatprintf(content, "# Sets the listen port for passive dcc transfers. This port needs to be forwared in your router, such that external TCP acknowledgements are not blocked.\n");
    content = sdscatprintf(content, "#listenPort=55554\n");
    
    sdsfree(downloadDir);

    return content;
}

static void writeDefaultConfigFile() {
    sds configDir = getConfigDirectory();
    sds configPath = sdscatprintf(sdsempty(), "%s%s", configDir, "config");

    sds content = getDefaultConfigContent();

    file_io_t *configFile = Open(configPath, "w");
    Write(configFile, content, sdslen(content));
    Close(configFile);

    sdsfree(content);

    sdsfree(configPath);
    sdsfree(configDir);
}

static void createDefaultConfigFile() {
    sds configDir = getConfigDirectory();
    if (!dir_exists(configDir)) {
        if (mkdir(configDir, 0755) == -1) {
            DBG_WARN("cant create dir %s", configDir);
            perror("mkdir");
        }
    }

    writeDefaultConfigFile();
    sdsfree(configDir);
}

static void parseConfigLine(struct xdccGetConfig *config, sds line) {
    int count, i;
    size_t j = 0;
    char *seperator = "=";
    sds *splitted = sdssplitlen(line, sdslen(line), seperator, strlen(seperator), &count);

    if (count != 2) {
        return;
    }

    for (i = 0; i < count; i++)
        sdstrim(splitted[i], " \t");

    sds type = splitted[0], value = splitted[1];
    DBG_OK("%s=%s", type, value);

    size_t numCallbacks = sizeof (configLineCallbacks) / sizeof (struct ConfigLineParser);

    for (; j < numCallbacks; j++) {
        struct ConfigLineParser *lineParser = &configLineCallbacks[j];
        if (str_equals(type, lineParser->type)) {
            lineParser->parse_line(config, value);
        }
    }

    sdsfreesplitres(splitted, count);
}

static inline bool ignoreLine(sds line) {
    return strcmp(line, "") == 0 || line[0] == '#';
}

static void parseConfigString(struct xdccGetConfig *config, sds content) {
    int count, i;
    char *seperator = "\n";
    sds *splitted = sdssplitlen(content, sdslen(content), seperator, strlen(seperator), &count);

    for (i = 0; i < count; i++) {
        sdstrim(splitted[i], " \r\t");
        if (ignoreLine(splitted[i])) {
            continue;
        }

        parseConfigLine(config, splitted[i]);
    }

    sdsfreesplitres(splitted, count);
}

void parseConfigFile(struct xdccGetConfig *config) {
    DBG_OK("in parseConfigFile");
    sds configDir = getConfigDirectory();
    sds configFilePath = sdscatprintf(sdsempty(), "%s%s", configDir, "config");

    if (!file_exists(configFilePath)) {
        DBG_OK("config file does not exist, need to create one!");
        createDefaultConfigFile();
    }

    sds content = readTextFile(configFilePath);
    parseConfigString(config, content);

    sdsfree(content);
    sdsfree(configDir);
    sdsfree(configFilePath);
}
