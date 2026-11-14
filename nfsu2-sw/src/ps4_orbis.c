/*
 * ps4_orbis.c -- PS4 boot glue for the NFSU2 recompilation (the counterpart of switch_nx.c).
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

/* ---- thread census -------------------------------------------------------------------------------------------
 * pthread_create fails with EAGAIN for EVERY thread (probes in build 8) once ~30 are alive, with memory to spare. Count who
 * creates them. `ps4_boot` offsets turn into ELF addresses with nm on the matching build. */
void ps4_boot(void);
extern int __real_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);
static volatile int s_threads_made, s_threads_live;
static __thread int t_counted;     /* this thread went through census_tramp */
extern void __real_pthread_exit(void *v) __attribute__((noreturn));

static void census_leave(void);
void __wrap_pthread_exit(void *v)
{
    census_leave();
    __real_pthread_exit(v);
}
struct census_start { void *(*fn)(void *); void *arg; };

/* Threads the profiler may sample (see "Sampling profiler" below). */
#define PROF_MAXT 64
static struct { pthread_t th; void *(*fn)(void *); volatile int live; } s_pt[PROF_MAXT];
static volatile int s_pt_n;
static __thread int t_slot = -1;

static void census_leave(void)
{
    if (t_slot >= 0)
        s_pt[t_slot].live = 0;
    if (t_counted) {
        t_counted = 0;
        __atomic_sub_fetch(&s_threads_live, 1, __ATOMIC_RELAXED);
    }
}

static void *census_tramp(void *p)
{
    struct census_start cs = *(struct census_start *)p;
    void *r;
    int slot;
    free(p);
    __atomic_add_fetch(&s_threads_live, 1, __ATOMIC_RELAXED);
    t_counted = 1;
    slot = __atomic_fetch_add(&s_pt_n, 1, __ATOMIC_RELAXED);
    if (slot < PROF_MAXT) {
        s_pt[slot].th = pthread_self();
        s_pt[slot].fn = cs.fn;
        s_pt[slot].live = 1;
        t_slot = slot;
    }
    r = cs.fn(cs.arg);
    census_leave();
    return r;
}

int __wrap_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg)
{
    struct census_start *cs = (struct census_start *)malloc(sizeof *cs);
    int n = __atomic_add_fetch(&s_threads_made, 1, __ATOMIC_RELAXED);
    int r;
    size_t stack = 0;
    void *caller = __builtin_return_address(0);
    if (a)
        pthread_attr_getstacksize(a, &stack);
    if (!cs)
        return __real_pthread_create(t, a, fn, arg);
    cs->fn = fn;
    cs->arg = arg;
    r = __real_pthread_create(t, a, census_tramp, cs);
    if (r != 0)
        free(cs);
    if (n <= 80 || r != 0)
        fprintf(ps4_log_file, "[THREAD] #%d create -> 0x%x (live now %d, stack %zu KiB), caller ps4_boot%+ld, start ps4_boot%+ld\n", n,
                (unsigned)r, (int)s_threads_live, stack / 1024, (long)((char *)caller - (char *)ps4_boot),
                (long)((char *)fn - (char *)ps4_boot));
    return r;
}

/* ---- Sampling profiler (nfsu2x_env.txt: RECOMP_PS4_PROFILE=1 or 2) ------------------------------------------------
 * The console has no profiler. A sampler thread decides every 100 ms which census threads are busy (over 10% of a core,
 * from their CPU clocks) and every ~1 ms sends each of them SIGUSR1 (pthread_kill); the handler, running on that thread,
 * records slot, time and the interrupted pc. Every 10 s the samples are appended to /data/nfsu2x/prof.bin (16-byte
 * records: u16 slot, u16 time in 10 ms units, u32 0, u64 absolute pc) and the log gets the slot -> entry table, the
 * module map and a per-thread CPU line. ~/ps4/prof_ps4.py turns the file into tables against the build's ELF.
 * Where the pc sits in the signal context is not assumed: a calibration thread spins in a known function and the
 * handler's view of it decides. Off by default; a signal may cut a blocking Sony call short, so it is a diagnostic. */
#define PROF_MAX 200000
typedef struct { uint16_t slot, when; uint32_t pad; uint64_t pc; } ProfSample;
static ProfSample s_ps[PROF_MAX];
static volatile unsigned s_ps_n;
static volatile int s_uc_word = -1;         /* index (u64 words) of the pc in the signal's ucontext */
static volatile int s_cal_on;
static volatile uint64_t s_cal[64];

/* Mode 2 targets only the game thread, after its first frame fence. */
static int s_prof_mode;
static pthread_mutex_t s_prof_target_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t s_prof_target;
static int s_prof_target_ready, s_prof_background;
_Thread_local volatile sig_atomic_t ps4_prof_main, ps4_prof_scope;
static unsigned s_prof_pc_count;
static Ps4ProfileSample s_prof_pcs[PS4_FRAME_PC_CAP];

static void prof_main_record(uint64_t pc)
{
    unsigned n = __atomic_load_n(&s_prof_pc_count, __ATOMIC_RELAXED);
    if (n < PS4_FRAME_PC_CAP) {
        __atomic_store_n(&s_prof_pcs[n].pc, pc, __ATOMIC_RELAXED);
        __atomic_store_n(&s_prof_pcs[n].scope, (uint32_t)ps4_prof_scope, __ATOMIC_RELAXED);
        __atomic_store_n(&s_prof_pc_count, n + 1, __ATOMIC_RELEASE);
    }
}

unsigned ps4_profile_take(Ps4ProfileSample samples[PS4_FRAME_PC_CAP])
{
    if (s_prof_mode != 2) return 0;
    if (!ps4_prof_main) {
        ps4_prof_main = 1;
        pthread_mutex_lock(&s_prof_target_lock);
        s_prof_target = pthread_self();
        s_prof_target_ready = 1;
        pthread_mutex_unlock(&s_prof_target_lock);
        return 0;
    }
    /* The handler can only append on this thread. Copy the prefix before resetting it. */
    unsigned n = __atomic_load_n(&s_prof_pc_count, __ATOMIC_ACQUIRE);
    for (unsigned i = 0; i < n; i++) {
        samples[i].pc = __atomic_load_n(&s_prof_pcs[i].pc, __ATOMIC_RELAXED);
        samples[i].scope = __atomic_load_n(&s_prof_pcs[i].scope, __ATOMIC_RELAXED);
    }
    __atomic_store_n(&s_prof_pc_count, 0, __ATOMIC_RELAXED);
    return n;
}

void ps4_profile_end_main(void)
{
    if (s_prof_mode != 2) return;
    pthread_mutex_lock(&s_prof_target_lock);
    s_prof_target_ready = 0;
    pthread_mutex_unlock(&s_prof_target_lock);
    ps4_prof_main = 0;
}

static void prof_main_background(int background)
{
    if (s_prof_mode != 2) return;
    pthread_mutex_lock(&s_prof_target_lock);
    s_prof_background = background;
    pthread_mutex_unlock(&s_prof_target_lock);
}

static void prof_handler(int sig, siginfo_t *si, void *uc)
{
    const uint64_t *w = (const uint64_t *)uc;
    (void)sig;
    (void)si;
    if (s_cal_on) {
        int i;
        for (i = 0; i < 64; i++)
            s_cal[i] = w[i];
        s_cal_on = 0;
        return;
    }
    if (s_uc_word >= 0) {
        if (s_prof_mode == 2) {
            if (ps4_prof_main) prof_main_record(w[s_uc_word]);
            return;
        }
        unsigned i = __atomic_fetch_add(&s_ps_n, 1, __ATOMIC_RELAXED);
        if (i < PROF_MAX) {
            s_ps[i].slot = (uint16_t)t_slot;
            s_ps[i].when = (uint16_t)((now_s() - s_t0) * 100.0);
            s_ps[i].pad = 0;
            s_ps[i].pc = w[s_uc_word];
        }
    }
}

static volatile int s_spin_stop;
__attribute__((noinline)) static void prof_spin(void)
{
    while (!s_spin_stop)
        __asm__ volatile("pause");
}
static void *prof_spin_thread(void *a)
{
    (void)a;
    prof_spin();
    return NULL;
}

static void prof_flush(void)
{
    static char line[2048];
    FILE *f;
    unsigned n = s_ps_n;
    int i, len;
    if (n > PROF_MAX)
        n = PROF_MAX;
    if (n && (f = fopen(PS4_DIR "/prof.bin", "ab"))) {
        fwrite(s_ps, sizeof s_ps[0], n, f);
        fclose(f);
    }
    s_ps_n = 0;
    len = snprintf(line, sizeof line, "[prof] %u samples written; slots:", n);
    for (i = 0; i < s_pt_n && i < PROF_MAXT && len < 1900; i++)
        len += snprintf(line + len, sizeof line - len, " %d=%+ld", i, (long)((char *)s_pt[i].fn - (char *)ps4_boot));
    fprintf(ps4_log_file, "%s\n", line);
}

static void *prof_thread(void *arg)
{
    static const char *cal_fail = "[prof] cannot find the pc in the signal context: profiler off\n";
    pthread_t cal;
    struct sigaction sa;
    uint64_t last_cpu[PROF_MAXT] = {0}, busy_us[PROF_MAXT] = {0};
    int busy[PROF_MAXT] = {0};
    int i, tick = 0, cpu_ok = 0, rr = 0;   /* cpu_ok: CPU clocks are useless here, see below */
    double t_flush = now_s(), t_busy = now_s();
    OrbisKernelModule mods[64];
    size_t nmods = 0;
    (void)arg;

    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = prof_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        fprintf(ps4_log_file, "[prof] could not install SIGUSR1 handler\n");
        return NULL;
    }

    /* calibration: where does this libc put the interrupted pc? */
    if (__real_pthread_create(&cal, NULL, prof_spin_thread, NULL) != 0) {
        fprintf(ps4_log_file, "[prof] no calibration thread\n");
        return NULL;
    }
    usleep(50 * 1000);
    s_cal_on = 1;
    pthread_kill(cal, SIGUSR1);
    for (i = 0; i < 50 && s_cal_on; i++)
        usleep(10 * 1000);
    s_spin_stop = 1;
    pthread_join(cal, NULL);
    for (i = 0; i < 64; i++) {
        uint64_t v = s_cal[i], f = (uint64_t)(uintptr_t)prof_spin;
        if (v >= f && v < f + 96) {
            s_uc_word = i;
            break;
        }
    }
    if (s_uc_word < 0) {
        fputs(cal_fail, ps4_log_file);
        for (i = 0; i < 40; i++)
            fprintf(ps4_log_file, "[prof] cal[%d]=%llx\n", i, (unsigned long long)s_cal[i]);
        return NULL;
    }
    fprintf(ps4_log_file, "[prof] pc is word %d of the ucontext; ps4_boot at %p\n", s_uc_word, (void *)ps4_boot);
    if (s_prof_mode == 1) {
        struct timespec ts;
        clockid_t cid;
        pthread_t me = pthread_self();
        if (pthread_getcpuclockid(me, &cid) != 0 || clock_gettime(cid, &ts) != 0) {
            fprintf(ps4_log_file, "[prof] no per-thread CPU clocks: sampling every thread round-robin\n");
            cpu_ok = 0;
        }
    }
    if (sceKernelGetModuleList(mods, 64, &nmods) == 0) {
        for (i = 0; i < (int)nmods && i < 64; i++) {
            static OrbisKernelModuleInfo mi;
            memset(&mi, 0, sizeof mi);
            mi.size = sizeof mi;
            if (sceKernelGetModuleInfo(mods[i], &mi) == 0 && mi.segmentCount) {
                unsigned k;
                fprintf(ps4_log_file, "[prof] module %s:", mi.name);
                for (k = 0; k < mi.segmentCount && k < 4; k++)
                    fprintf(ps4_log_file, " %p+%x", mi.segmentInfo[k].address, mi.segmentInfo[k].size);
                fprintf(ps4_log_file, "\n");
            }
        }
    }
    if (s_prof_mode == 2) {
        uint32_t jitter = 1;
        fprintf(ps4_log_file, "[prof] game thread only, every 6–10 ms; slow-frame PCs in this log\n");
        fprintf(ps4_log_file, "[prof] hitch format PC@scope (hex): host nibble, phase nibble, 3 Xbox ordinal digits\n");
        fprintf(ps4_log_file, "[prof] host: 1 mutex-lock, 2 mutex-unlock, 3 cond-wait, 4 cond-timedwait, 5 cond-wake,"
                " 6 monotonic-clock, 7 realtime-clock, 8 yield, 9 sleep, a getrusage; phase: 1 guest-lock acquire,"
                " 2 guest-lock wait, 3 guest-lock release, 4 frame fence; 0 is untagged\n");
        for (;;) {
            jitter = jitter * 1664525u + 1013904223u;
            usleep(6000u + ((jitter >> 16) % 4001u));
            pthread_mutex_lock(&s_prof_target_lock);
            if (s_prof_target_ready && !s_prof_background)
                pthread_kill(s_prof_target, SIGUSR1);
            pthread_mutex_unlock(&s_prof_target_lock);
        }
    }
    for (;;) {
        double now;
        usleep(1000);
        now = now_s();
        if (now - t_busy >= 0.1) {                  /* who is busy */
            for (i = 0; i < s_pt_n && i < PROF_MAXT; i++) {
                struct timespec ts;
                clockid_t cid;
                busy[i] = 0;
                if (!s_pt[i].live)
                    continue;
                if (!cpu_ok) {
                    busy[i] = 1;
                    continue;
                }
                if (pthread_getcpuclockid(s_pt[i].th, &cid) == 0 && clock_gettime(cid, &ts) == 0) {
                    uint64_t us = (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
                    busy[i] = (us - last_cpu[i]) * 10 > (uint64_t)((now - t_busy) * 1.0e6);
                    busy_us[i] += us - last_cpu[i];
                    last_cpu[i] = us;
                }
            }
            t_busy = now;
        }
        /* One thread per tick, round-robin over the live ones. (Build 15 signalled every "busy" thread each ms, but this
         * system's per-thread CPU clocks read ~99% for all of them: 18 signals per ms, 1.2 fps.) */
        if (s_pt_n > 0 && s_ps_n < PROF_MAX) {
            int k, n = s_pt_n < PROF_MAXT ? s_pt_n : PROF_MAXT;
            for (k = 0; k < n; k++) {
                int j = (rr + k) % n;
                if (s_pt[j].live) {
                    pthread_kill(s_pt[j].th, SIGUSR1);
                    rr = j + 1;
                    break;
                }
            }
        }
        if (now - t_flush >= 10.0) {
            char cl[1024];
            int len = snprintf(cl, sizeof cl, "[prof] cpu%% of a core over the last %.0f s:", now - t_flush);
            for (i = 0; i < s_pt_n && i < PROF_MAXT && len < 950; i++) {
                if (busy_us[i] * 50 > (uint64_t)((now - t_flush) * 1.0e6))
                    len += snprintf(cl + len, sizeof cl - len, " %d=%.0f", i, busy_us[i] / ((now - t_flush) * 1.0e4));
                busy_us[i] = 0;
            }
            fprintf(ps4_log_file, "%s\n", cl);
            prof_flush();
            t_flush = now;
        }
        tick++;
    }
    return NULL;
}

static void prof_start(void)
{
    const char *e = getenv("RECOMP_PS4_PROFILE");
    pthread_t t;
    if (!e || (strcmp(e, "1") && strcmp(e, "2")))
        return;
    s_prof_mode = atoi(e);
    if (s_prof_mode == 1)
        remove(PS4_DIR "/prof.bin");
    if (__real_pthread_create(&t, NULL, prof_thread, NULL) == 0)
        pthread_detach(t);
    else
        fprintf(ps4_log_file, "[prof] could not start the profiler thread\n");
}

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
    /* The pinned driver treats any defined value, including "0", as on. */
    const char *submit_done = getenv("ORBIS_SUBMIT_DONE");
    if (submit_done && strcmp(submit_done, "0") == 0)
        unsetenv("ORBIS_SUBMIT_DONE");
    fprintf(ps4_log_file, "[ps4] GPU frame completion notification %s\n",
            getenv("ORBIS_SUBMIT_DONE") ? "on" : "off");
    ps4_timing_probe();
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
