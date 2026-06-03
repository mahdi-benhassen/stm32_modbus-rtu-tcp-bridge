#include <sys/stat.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <errno.h>
#include <stdint.h>

/*
 * ============================================================
 *  Newlib Syscall Stubs (Minimal)
 *
 *  Provides the minimal set of system call implementations
 *  required by newlib-nano (used with --specs=nano.specs).
 *  This avoids pulling in the full libnosys.
 *
 *  STM32F407 has no OS-level file I/O; all stubs return errors
 *  or dummy values appropriate for a bare-metal/FreeRTOS system.
 * ============================================================ */

/* ---- Memory Management ---- */

caddr_t _sbrk(int incr)
{
    extern char end __asm__("end");
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == NULL) {
        heap_end = &end;
    }

    prev_heap_end = heap_end;

    /* Simple heap: grows from end of BSS toward stack.
     * No check - FreeRTOS heap_4.c manages its own memory. */
    heap_end += incr;

    return (caddr_t)prev_heap_end;
}

/* ---- File I/O (all unsupported) ---- */

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    (void)st;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    errno = EBADF;
    return -1;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    return len;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

/* ---- Timing ---- */

int _times(struct tms *buf)
{
    (void)buf;
    return -1;
}

clock_t _clock(void)
{
    return (clock_t)-1;
}

int _gettimeofday(struct timeval *tv, void *tz)
{
    (void)tv;
    (void)tz;
    errno = ENOSYS;
    return -1;
}

/* ---- Environment ---- */

char *__env[1] = { 0 };
char **environ = __env;
