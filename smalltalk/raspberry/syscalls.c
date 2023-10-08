#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef BROKEN_GETTIMEOFDAY
#include <circle/bcm2835.h>
#else
// use circle's GetTime so that NTP affects the C library time() function
extern unsigned syscall_gettimeofday(void);
#endif

void _exit(int status) {
    while(1);
}

int _fstat(int file, struct stat *st) {
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_mode = 0;
    st->st_nlink = 0;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = 0;
    st->st_atime = st->st_mtime = st->st_ctime = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    return 0;
}

int _open(const char *name, int flags, int mode) {
    return -1;
}

static char * heap_end = 0;

caddr_t _sbrk(int incr) {
    extern char __heap_start; /* Defined by the linker */
    extern char __heap_end; /* Defined by the linker */
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &__heap_start;
    }
    prev_heap_end = heap_end;

    if (heap_end + incr > &__heap_end) {
        /* Heap and stack collision */
        return (caddr_t) 0;
    }

    heap_end += incr;

    return (caddr_t) prev_heap_end;
}

int _write(int file, char *ptr, int len) {
    return -1;
}

int _close(int file) {
    return -1;
}

int _gettimeofday(struct timeval *tv, struct timezone *tz) {
#ifdef BROKEN_GETTIMEOFDAY
    // uint64_t t = *((uint64_t *)0x20003004);  // get uptime in nanoseconds
    uint64_t t = *((uint64_t *)ARM_SYSTIMER_CLO);  // get uptime in nanoseconds
    tv->tv_sec = t / 1000000;  // convert to seconds
    tv->tv_usec = ( t % 1000000 ) / 1000;  // get remaining microseconds
#else
    tv->tv_sec = syscall_gettimeofday();
    tv->tv_usec = 0;
#endif
    // TODO better delegate to GetEpochTime();
    // TODO fill in *tz with GMT 0 info, if non-NULL
    return 0;  // return non-zero for error
}

int _isatty(int file) {
    return 0;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _read(int file, char *ptr, int len) {
    return -1;
}

void _exit(int code)  _ATTRIBUTE ((__noreturn__));

int _kill(pid_t pid, int sig) {
    return -1;
}

pid_t _getpid(void) {
    return -1;
}

