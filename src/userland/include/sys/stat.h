#ifndef SYS_STAT_H
#define SYS_STAT_H

struct stat {
    unsigned int st_size;
    unsigned int st_mode;
};

int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);

#endif
