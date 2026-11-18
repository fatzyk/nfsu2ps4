/*
 * ps4_orbis.c -- PS4 boot glue for the NFSU2 recompilation.
 *
 *   settings  /data/nfsu2x/nfsu2x_env.txt, KEY=VALUE per line, '#' comments (a console has no environment)
 *   closing   background execution also means the Home screen, so it must never trigger an exit timer.
 *             exit() and returning from main() end in SIGSYS and a crash dialog: nothing here calls them.
 *   paths     absolute only, /data/nfsu2x/...
 *
 * The orbis SDK headers are only included here, not in the runtime: they collide with the Win32 vocabulary.
 */
#ifdef __PS4__

#define PS4_NO_STDIO_REDIRECT 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include "../xboxrecomp/src/platform/ps4_timing.h"

#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

#define PS4_DIR      "/data/nfsu2x"
#define PS4_ENV      PS4_DIR "/nfsu2x_env.txt"

int   mcpx_apu_frames_per_second(void);      /* xboxrecomp/src/apu */
float mcpx_apu_utilization(void);
typedef struct { uint64_t submitted, dropped, underruns, clipped, samples; int peak; } Xa2Stats;   /* apu_xaudio2.h */
void xa2_get_stats(Xa2Stats *out);
typedef int (*avail_dm_fn)(long long, long long, size_t, long long *, size_t *);
void nv2a_vk_pool_report(char *buf, size_t n) __attribute__((weak));
typedef int (*avail_flex_fn)(size_t *);

FILE *ps4_log_file;                 /* what stdout and stderr became (ps4_stdio.h) */

static double s_t0;

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9;
}

static void __attribute__((constructor)) early_log(void)
{
    ps4_log_file = stderr;
}

void ps4_flush_log(void)
{
    if (ps4_log_file)
        fflush(ps4_log_file);
}

static void load_env(void)
{
    FILE *f = fopen(PS4_ENV, "r");
    char line[1024];

    if (!f) {
        fprintf(ps4_log_file, "[ps4] no %s (defaults)\n", PS4_ENV);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *eq, *end = line + strlen(line);
        while (end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' '))
            *--end = 0;
        if (!line[0] || line[0] == '#' || !(eq = strchr(line, '=')) || eq == line)
            continue;
        *eq = 0;
        setenv(line, eq + 1, 1);
        fprintf(ps4_log_file, "[ps4] env %s=%s\n", line, eq + 1);
    }
    fclose(f);
}

/* The Vulkan renderer calls this (weakly) after every presented frame. Defining it matters: an undefined weak
 * symbol makes create-fself fail with "missing library for symbol". It also feeds the fps line below. */
static volatile unsigned s_frames;
void nv2a_host_frame_presented(uint32_t width, uint32_t height)
{
    (void)width;
    (void)height;
    s_frames++;
}

_Thread_local volatile sig_atomic_t ps4_prof_main, ps4_prof_scope;
unsigned ps4_profile_take(Ps4ProfileSample samples[PS4_FRAME_PC_CAP])
{
    (void)samples;
    return 0;
}
void ps4_profile_end_main(void) {}
typedef int (*status_fn)(void *);
typedef int (*event_fn)(void *);

static volatile sig_atomic_t s_close_signal;

static void close_signal(int sig)
{
    s_close_signal = sig;
}

/* Drain lifecycle events without treating the Home screen as a close request. */
static void *watcher(void *arg)
{
    int splash_hidden = 0;
    (void)arg;
    for (;;) {
        static unsigned char st[256];
        static unsigned char event[8196] __attribute__((aligned(16)));
        usleep(100 * 1000);
        memset(st, 0, sizeof st);
        if (((status_fn)sceSystemServiceGetStatus)(st) == 0 &&
            (st[0] || st[1] || st[2] || st[3])) {
            memset(event, 0, sizeof event);
            ((event_fn)sceSystemServiceReceiveEvent)(event);
        }
        if (s_close_signal)
            kill(getpid(), SIGKILL);
        if (!splash_hidden && (s_frames || now_s() - s_t0 >= 20.0)) {
            sceSystemServiceHideSplashScreen();
            splash_hidden = 1;
        }
    }
    return NULL;
}

/* Probe 1 opened the pad on the main thread and it worked. In build 3 the same calls ran lazily on the USB emulation
 * thread, whose stack lives in guest memory (a direct-memory mapping): sceUserServiceGetInitialUser failed with
 * 0x80960002 and scePadOpen with 0x809B0001, so no button ever reached the game. The system libraries are
 * called from the main thread here, once, and everything they write to lives in static buffers. */
static int s_pad_handle = -1;

int ps4_pad_get_handle(void)
{
    return s_pad_handle;
}

static void pad_open(void)
{
    int32_t user = -1;
    int r1 = sceUserServiceInitialize(NULL);
    int r2 = sceUserServiceGetInitialUser(&user);
    int r3 = scePadInit();
    s_pad_handle = scePadOpen(user, 0, 0, NULL);
    fprintf(ps4_log_file, "[ps4] user service init 0x%x, initial user %d (0x%x), scePadInit 0x%x, scePadOpen -> %d (0x%x)\n",
            (unsigned)r1, (int)user, (unsigned)r2, (unsigned)r3, s_pad_handle, (unsigned)s_pad_handle);
}

void ps4_boot(void)
{
    pthread_t t;

    s_t0 = now_s();
    ps4_log_file = stderr;
    mkdir(PS4_DIR, 0777);
    mkdir(PS4_DIR "/cache", 0777);
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = close_signal;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
    }
    fprintf(ps4_log_file, "[ps4] ps4_boot at %p (subtract from a crash address to get an ELF offset)\n", (void *)ps4_boot);
    setenv("MESA_SHADER_CACHE_DISABLE", "true", 0);   /* no util_queue thread for Mesa's disk cache */
    /* Defaults (nfsu2x_env.txt overrides). The profile of build 16 showed the game thread busy ~40% and the executor ~70% in
     * races: they wait for each other. The two switches below are upstream's answers to exactly that (PERF_NOTES.md). */
    setenv("RECOMP_VK_THREAD", "1", 0);               /* Vulkan recording on its own thread (build 19) */
    setenv("RECOMP_FRAME_LAG", "1", 0);
    setenv("RECOMP_GIL_EAGER", "1", 0);
    setenv("NFSU2_FPS_CAP", "30", 0);
    setenv("RECOMP_VK_WARM_TEX", "1", 0);
    setenv("ORBIS_SUBMIT_DONE", "1", 0);
    load_env();
    setenv("RECOMP_TIMING", "0", 1);
    unsetenv("RECOMP_APU_RING_STATS");
    unsetenv("RECOMP_PS4_PROFILE");
    /* The pinned driver treats any defined value, including "0", as on. */
    const char *submit_done = getenv("ORBIS_SUBMIT_DONE");
    if (submit_done && strcmp(submit_done, "0") == 0)
        unsetenv("ORBIS_SUBMIT_DONE");
    fprintf(ps4_log_file, "[ps4] GPU frame completion notification %s\n",
            getenv("ORBIS_SUBMIT_DONE") ? "on" : "off");

    pad_open();
    if (pthread_create(&t, NULL, watcher, NULL) == 0)
        pthread_detach(t);
    else
        fprintf(ps4_log_file, "[ps4] could not start the watcher thread\n");
    fflush(ps4_log_file);
}

/* The way out of the process. The guest asks for it (HalReturnToFirmware, XLaunchNewImage...), the runtime calls
 * ExitProcess(), a fatal error calls abort(): all of them end here, because exit() would end in SIGSYS and a crash
 * dialog. LoadExec("exit") is the clean way (probe 5), SIGKILL the fallback. */
void ps4_exit(int code)
{
    int i;
    ps4_profile_end_main();
    fprintf(ps4_log_file, "[ps4] t=%.1f exit(%d) requested: leaving the app with LoadExec(\"exit\")\n", now_s() - s_t0, code);
    fflush(ps4_log_file);
    sceSystemServiceLoadExec("exit", NULL);
    for (i = 0; i < 30; i++)
        usleep(100 * 1000);
    fprintf(ps4_log_file, "[ps4] still alive 3 s after LoadExec(exit): SIGKILL\n");
    fflush(ps4_log_file);
    kill(getpid(), SIGKILL);
    for (;;)
        sleep(1);
}

/* Never return from main(): returning ends in SIGSYS and a crash dialog on this console. */
void ps4_idle_forever(void)
{
    fprintf(ps4_log_file, "[ps4] idling until the system closes the app\n");
    fflush(ps4_log_file);
    for (;;)
        sleep(1);
}

#endif /* __PS4__ */
