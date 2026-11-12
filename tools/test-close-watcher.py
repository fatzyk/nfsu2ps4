#!/usr/bin/env python3
"""Run the actual watcher with mocked console calls and accelerated time."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "nfsu2-sw/src/ps4_orbis.c").read_text()
watcher = source.split("typedef int (*status_fn)(void *);", 1)[1].split("/* Probe 1 opened", 1)[0]
preamble = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <assert.h>
typedef int (*status_fn)(void *);
typedef int (*avail_dm_fn)(long long,long long,size_t,long long*,size_t*);
typedef int (*avail_flex_fn)(size_t*);
typedef struct { uint64_t submitted,dropped,underruns,clipped,samples; int peak; } Xa2Stats;
static FILE *ps4_log_file;
static double s_t0;
static unsigned s_frames;
static int s_threads_made,s_threads_live,tick,want_signal,kills,events;
static int backgrounded, foregrounded;
static jmp_buf done;
static void close_signal(int sig);
static double now_s(void) { return tick/10.0; }
static void prof_main_background(int bg) {
    if (bg) ++backgrounded;
    else ++foregrounded;
}
static int fake_usleep(unsigned usec) {
    (void)usec;
    if (++tick == 220 && want_signal) close_signal(SIGTERM);
    if (tick == 250) longjmp(done,1);
    return 0;
}
#define usleep fake_usleep
static int fake_kill(int pid,int sig) {
    (void)pid; assert(sig == SIGKILL); ++kills; longjmp(done,1); return 0;
}
#define kill fake_kill
static int sceSystemServiceGetStatus(void *p) {
    unsigned char *s=p;
    if (tick==100) return -1;
    s[4] = tick >= 10 && tick < 20;
    s[5] = tick >= 20 && tick < 200;
    if (tick==200) s[0]=1;
    return 0;
}
static int sceSystemServiceReceiveEvent(void *p) { memset(p,0,8196); ++events; return 0; }
static int sceSystemServiceHideSplashScreen(void) { return 0; }
static int mcpx_apu_frames_per_second(void) { return 1500; }
static float mcpx_apu_utilization(void) { return 0; }
static int sceKernelAvailableFlexibleMemorySize(size_t *p) { *p=0; return 0; }
static int sceKernelAvailableDirectMemorySize(long long a,long long b,size_t c,long long *d,size_t *e) {
    (void)a;(void)b;(void)c; *d=0; *e=0; return 0;
}
static long long sceKernelGetDirectMemorySize(void) { return 0; }
static void (*nv2a_vk_pool_report)(char *,size_t);
static void xa2_get_stats(Xa2Stats *s) { memset(s,0,sizeof *s); }
'''
main = r'''
int main(int argc,char **argv) {
    (void)argv; want_signal=argc>1;
    ps4_log_file=tmpfile(); assert(ps4_log_file);
    if (!setjmp(done)) watcher(NULL);
    assert(events==1);
    assert(backgrounded==1 && foregrounded==1);
    assert(kills==want_signal);
    assert(tick==(want_signal ? 220 : 250));
    fclose(ps4_log_file);
    puts(want_signal ? "PASS: explicit termination signal exits" : "PASS: Home for 18 seconds and return does not exit");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory)
    (path / "watcher.c").write_text(preamble + watcher + main)
    subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", str(path / "watcher.c"), "-o", str(path / "watcher")], check=True)
    subprocess.run([str(path / "watcher")], check=True)
    subprocess.run([str(path / "watcher"), "signal"], check=True)
