#ifndef FILE_H
#define	FILE_H

#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "helper.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

struct file_io_t {
#ifdef FILE_API
    FILE *fd;
#else
    int fd;
#endif
    const char *fileName;
    char *mode;
};    

typedef struct file_io_t file_io_t;
typedef void (*FileReader) (void *buffer, unsigned int bytesRead, void *ctx);

static inline bool file_exists(char *file) {
#ifndef _MSC_VER
    int ret = access( file, F_OK );
    return ret == 0;
#else
    DWORD dwAttrib = GetFileAttributes(file);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#endif
}

static inline bool dir_exists(char *dir) {
    struct stat s;
    int err = stat(dir, &s);
    
    if (err == -1) {
        if (errno == ENOENT) {
            return false;
        }
        
        perror("stat");
        return false;
    }
    
    if(S_ISDIR(s.st_mode)) {
        return true;
    }
    
    return false;
}

irc_dcc_size_t get_file_size(char* filename);

#ifdef FILE_API
static inline size_t Read (file_io_t *fd, void *buf, size_t count) {
#else
static inline ssize_t Read (file_io_t *fd, void *buf, ssize_t count) {
#endif
#ifdef FILE_API
    size_t readBytes = fread(buf, 1, count, fd->fd);
    return readBytes;
#else
    ssize_t readBytes = 0;

    do {
        ssize_t ret = read(fd->fd, buf + readBytes, count-readBytes);
        if (unlikely(ret == -1)) {
            logprintf(LOG_ERR, "Cant read the file %s. Exiting now.", fd->fileName);
            exitPgm(EXIT_FAILURE);
        }
        else if (ret == 0) {
            return readBytes;
        }

        readBytes += ret;
    } while (readBytes < count);

    return readBytes;
#endif
}

#ifdef FILE_API
static inline void Write(file_io_t *fd, const void *buf, size_t count) {
#else
  static inline void Write(file_io_t *fd, const void *buf, ssize_t count) {
#endif
#ifdef FILE_API
    size_t written = fwrite(buf, 1, count, fd->fd);
    if (unlikely(written != count)) {
        logprintf(LOG_ERR, "Cant write the file %s. Exiting now.", fd->fileName);
        exitPgm(EXIT_FAILURE);
    }
#else
    ssize_t written = 0;
    do {
        ssize_t ret = write(fd->fd, buf + written, count-written);
        written += ret;
        if (unlikely(ret == -1)) {
            logprintf(LOG_ERR, "Cant write the file %s. Exiting now.", fd->fileName);
            exitPgm(EXIT_FAILURE);
        }
    } while(written < count);
#endif
}
    
file_io_t* Open(const char *pathname, char *mode);
void Close(file_io_t *fd);
void Seek(file_io_t *fd, uint64_t offset, int whence);
void readFile(char *filename, FileReader callback, void *ctx);

#endif	/* FILE_H */

