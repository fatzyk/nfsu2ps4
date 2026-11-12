#!/usr/bin/env python3
"""Exercise GPU query handling and CPU clock validation; requires Linux cc."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / "nfsu2-sw/xboxrecomp/src/nv2a_vk/nv2a_vk.c").read_text()
helpers = source.split("static uint32_t s_timestamp_bits;", 1)[1].split("static void garbage_add_pipe", 1)[0]
prefix = r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
typedef int VkResult;
#define VK_SUCCESS 0
#define VK_QUERY_RESULT_64_BIT 1
#define VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT 1
#define VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT 2
static int s_dev, s_cb, query_rc, resets, starts, ends;
static struct { int timing; } frame = {1}, *s_f = &frame;
static uint64_t ticks[2], now = 1;
static char report[1024];
static uint64_t hz_now(void) { return now; }
static int capture(FILE *f, const char *fmt, ...) {
    (void)f; va_list args; va_start(args, fmt);
    int n = vsnprintf(report, sizeof report, fmt, args); va_end(args); return n;
}
#define fprintf capture
static void vkCmdResetQueryPool(int c, int q, int first, int n) {
    (void)c; assert(q == 1 && first == 0 && n == 2); resets++;
}
static void vkCmdWriteTimestamp(int c, int stage, int q, int index) {
    (void)c; assert(q == 1);
    if (stage == 1) { assert(index == 0); starts++; }
    else { assert(stage == 2 && index == 1); ends++; }
}
static int vkGetQueryPoolResults(int d, int q, int first, int n, size_t bytes,
    void *out, size_t stride, int flags) {
    (void)d; assert(q == 1 && !first && n == 2 && bytes == 16 && stride == 8);
    assert(flags == VK_QUERY_RESULT_64_BIT); /* No added wait flag. */
    memcpy(out, ticks, sizeof ticks); return query_rc;
}
static uint32_t s_timestamp_bits;
'''
suffix = r'''
int main(void) {
    s_timestamp_bits = 8; s_timestamp_ns = 1000000;
    timing_begin(); timing_end();
    assert(resets == 1 && starts == 1 && ends == 1);
    ticks[0] = 250; ticks[1] = 4; timing_read(); /* wrapped duration: 10 ms */
    query_rc = 1; now += 10000000000ull; timing_read();
    assert(strstr(report, "avg 10.00 ms max 10.00 ms (1 samples, 1 missing)"));
    assert(s_timing_wait_ns == 0);
    query_rc = 0; s_timestamp_bits = 64;
    ticks[0] = UINT64_MAX - 4; ticks[1] = 5;
    s_timing_wait_ns = 2000000; now += 10000000000ull; timing_read();
    assert(strstr(report, "avg 10.00 ms max 10.00 ms (1 samples, 0 missing)"));
    assert(strstr(report, "wait 2.0 ms total"));
    s_f->timing = 0; timing_begin(); timing_end(); timing_read();
    assert(resets == 1 && starts == 1 && ends == 1);
    puts("PASS: timestamp wrap, query failure, reset, disabled timing, no extra GPU wait");
}
'''
with tempfile.TemporaryDirectory() as temp:
    c = pathlib.Path(temp) / "timing.c"
    exe = pathlib.Path(temp) / "timing"
    c.write_text(prefix + helpers + suffix)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

bridge = (root / "nfsu2-sw/xboxrecomp/src/kernel/kernel_bridge.c").read_text()
cpu_helpers = bridge.split("uint64_t ps4_timing_now_ns(void)", 1)[1].split("static void main_timing_report", 1)[0]
platform = (root / "nfsu2-sw/src/ps4_orbis.c").read_text()
profile_helpers = platform.split("static int s_prof_mode;", 1)[1].split("static void prof_handler", 1)[0]
cpu_test = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include "ps4_timing.h"
#define RECOMP_TLS _Thread_local
static int s_cpu_clock_ok, s_timing_enabled, t_gil_main, wall_as_cpu, unsupported;
static uint64_t wall_us, sleep_us;
static int fake_clock_gettime(clockid_t clock, struct timespec *ts) {
    assert(clock == CLOCK_MONOTONIC);
    wall_us += 1000;
    *ts = (struct timespec){wall_us / 1000000, wall_us % 1000000 * 1000};
    return 0;
}
static int fake_getrusage(int who, struct rusage *r) {
    assert(who == RUSAGE_THREAD);
    if (unsupported) return -1;
    r->ru_utime.tv_usec = wall_us - (wall_as_cpu ? 0 : sleep_us);
    return 0;
}
static void fake_usleep(unsigned us) { wall_us += us; sleep_us += us; }
#define clock_gettime fake_clock_gettime
#define getrusage fake_getrusage
#define usleep fake_usleep
static int s_prof_mode;
''' + profile_helpers + r'''
uint64_t ps4_timing_now_ns(void)
''' + cpu_helpers + r'''
int main(void) {
    assert(ps4_cpu_ms(0, 2000000) == 2);
    assert(ps4_cpu_ms(2, 1) == -1);
    assert(ps4_cpu_ms(UINT64_MAX, 1) == -1);
    assert(setenv("RECOMP_TIMING", "1", 1) == 0);
    ps4_timing_probe();
    assert(s_cpu_clock_ok && ps4_thread_cpu_ns() != UINT64_MAX);

    Ps4MainFrameTiming frame;
    assert(ps4_main_fence_begin() == 0); /* Ignore other threads. */
    t_gil_main = 1;
    ps4_main_fence_end(ps4_main_fence_begin());
    ps4_main_frame_snapshot(&frame);
    assert(frame.end == 0); /* First fence only establishes a baseline. */
    wall_us += 19000;
    uint64_t start = ps4_main_fence_begin();
    fake_usleep(3000);
    s_main_frame_lock_ns = 2000000;
    ps4_main_fence_end(start);
    ps4_main_frame_snapshot(&frame);
    assert(frame.end && frame.span == 24000000 && frame.lock == 2000000 && frame.fence == 4000000);
    assert(frame.work_cpu_ms == 19 && frame.fence_cpu_ms == 2);
    assert(s_main_frame_lock_ns == 0);
    assert(pthread_mutex_lock(&s_main_frame_lock) == 0);
    ps4_main_frame_snapshot(&frame);
    assert(frame.end == 0); /* Skip a busy publisher without blocking. */
    assert(pthread_mutex_unlock(&s_main_frame_lock) == 0);

    s_prof_mode = 2;
    assert(ps4_profile_enter(PS4_PROF_PHASE_MASK, PS4_PROF_GIL_WAIT) == UINT32_MAX);
    ps4_main_fence_end(ps4_main_fence_begin()); /* Register the target. */
    assert(s_prof_target_ready && pthread_equal(s_prof_target, pthread_self()));
    uint32_t ordinal = ps4_profile_enter(PS4_PROF_ORD_MASK, 233);
    uint32_t phase = ps4_profile_enter(PS4_PROF_PHASE_MASK, PS4_PROF_GIL_WAIT);
    uint32_t host = ps4_profile_enter(PS4_PROF_HOST_MASK, PS4_PROF_HOST_COND_TIMEDWAIT);
    prof_main_record(100);
    ps4_profile_leave(host);
    prof_main_record(101);
    ps4_profile_leave(phase);
    prof_main_record(102);
    ps4_profile_leave(ordinal);
    assert(ps4_prof_scope == 0);
    for (unsigned i = 3; i <= PS4_FRAME_PC_CAP; i++)
        prof_main_record(100 + i);
    ps4_main_fence_end(ps4_main_fence_begin());
    ps4_main_frame_snapshot(&frame);
    assert(frame.pc_count == PS4_FRAME_PC_CAP);
    assert(frame.pcs[0].pc == 100 && frame.pcs[PS4_FRAME_PC_CAP - 1].pc == 100 + PS4_FRAME_PC_CAP - 1);
    assert(frame.pcs[0].scope == (233 | PS4_PROF_GIL_WAIT | PS4_PROF_HOST_COND_TIMEDWAIT));
    assert(frame.pcs[1].scope == (233 | PS4_PROF_GIL_WAIT));
    assert(frame.pcs[2].scope == 233 && frame.pcs[3].scope == 0);
    assert(ps4_profile_take(frame.pcs) == 0);
    prof_main_background(1);
    assert(s_prof_background);
    ps4_profile_end_main();
    assert(!s_prof_target_ready);

    wall_as_cpu = 1;
    ps4_timing_probe();
    assert(!s_cpu_clock_ok && ps4_thread_cpu_ns() == UINT64_MAX);
    unsupported = 1;
    ps4_timing_probe();
    assert(!s_cpu_clock_ok && ps4_thread_cpu_ns() == UINT64_MAX);
    puts("PASS: CPU clock, frame timing, bounded PC capture and nested call tags");
}
'''
with tempfile.TemporaryDirectory() as temp:
    c = pathlib.Path(temp) / "cpu-timing.c"
    exe = pathlib.Path(temp) / "cpu-timing"
    c.write_text(cpu_test)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread",
                    "-I", str(root / "nfsu2-sw/xboxrecomp/src/platform"),
                    str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
