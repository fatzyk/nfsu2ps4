/* ps4_stdio.h -- force-included (-include) into every PS4 translation unit.
 *
 * The runtime and the game print with printf/fprintf(stderr, ...) everywhere. On the PS4 there is no console to
 * read them from, and orbis-compat's freopen/dup2 route is untested on hardware, so the one stream that is known to
 * work (fopen + fprintf + fflush, probes 1-4) is used for everything: stdout and stderr both become
 * `ps4_log_file`, opened by ps4_boot() on /data/nfsu2x/nfsu2x.log with a big buffer that a thread flushes twice a
 * second. One stream also keeps the lines of the two in order.
 *
 * <stdio.h> is included first so its own declarations are not mangled by the macros below.
 * ps4_orbis.c defines PS4_NO_STDIO_REDIRECT and sees the real streams.
 */
#ifndef PS4_STDIO_H
#define PS4_STDIO_H

#include <stdio.h>
#include <stdarg.h>

#ifndef PS4_NO_STDIO_REDIRECT

#ifdef __cplusplus
extern "C" {
#endif
extern FILE *ps4_log_file;
#ifdef __cplusplus
}
#endif

#undef stdout
#undef stderr
#define stdout ps4_log_file
#define stderr ps4_log_file

#undef printf
#define printf(...)      fprintf(ps4_log_file, __VA_ARGS__)
#undef vprintf
#define vprintf(f, ap)   vfprintf(ps4_log_file, (f), (ap))
#undef puts
#define puts(s)          (fputs((s), ps4_log_file), fputc('\n', ps4_log_file))
#undef putchar
#define putchar(c)       fputc((c), ps4_log_file)

/* exit()/_exit()/abort() end in SIGSYS inside _exit and a crash dialog (orbis-compat README, probe 5). */
#include <stdlib.h>
#include <unistd.h>     /* declares _exit: must be seen before the macro below */
#ifdef __cplusplus
extern "C"
#endif
void ps4_exit(int code) __attribute__((noreturn));
#undef exit
#define exit(c)          ps4_exit((c))
#undef _exit
#define _exit(c)         ps4_exit((c))
#undef _Exit
#define _Exit(c)         ps4_exit((c))
#undef abort
#define abort()          ps4_exit(134)

#endif /* PS4_NO_STDIO_REDIRECT */
#endif /* PS4_STDIO_H */
