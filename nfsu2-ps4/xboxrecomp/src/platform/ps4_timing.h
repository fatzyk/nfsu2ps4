#ifndef RECOMP_PS4_TIMING_H
#define RECOMP_PS4_TIMING_H

#include <stdint.h>
#include <signal.h>

void ps4_timing_probe(void);
int ps4_timing_enabled(void);
uint64_t ps4_timing_now_ns(void);
uint64_t ps4_thread_cpu_ns(void);
uint64_t ps4_main_fence_begin(void);
void ps4_main_fence_end(uint64_t start);
uint64_t nv2a_pb_refresh_wait_ns(void);

#define PS4_FRAME_PC_CAP 16
typedef struct { uint64_t pc; uint32_t scope; } Ps4ProfileSample;
unsigned ps4_profile_take(Ps4ProfileSample samples[PS4_FRAME_PC_CAP]);
void ps4_profile_end_main(void);

/* The low 12 bits hold the Xbox kernel ordinal. Runtime phase and host call
 * occupy the next two nibbles so a nested wait keeps its caller's context. */
enum {
    PS4_PROF_ORD_MASK = 0x00fff,
    PS4_PROF_PHASE_MASK = 0x0f000,
    PS4_PROF_GIL_ACQUIRE = 0x01000, PS4_PROF_GIL_WAIT = 0x02000,
    PS4_PROF_GIL_RELEASE = 0x03000, PS4_PROF_FRAME_FENCE = 0x04000,
    PS4_PROF_HOST_MASK = 0xf0000,
    PS4_PROF_HOST_MUTEX_LOCK = 0x10000, PS4_PROF_HOST_MUTEX_UNLOCK = 0x20000,
    PS4_PROF_HOST_COND_WAIT = 0x30000, PS4_PROF_HOST_COND_TIMEDWAIT = 0x40000,
    PS4_PROF_HOST_COND_WAKE = 0x50000, PS4_PROF_HOST_MONOTONIC = 0x60000,
    PS4_PROF_HOST_REALTIME = 0x70000, PS4_PROF_HOST_YIELD = 0x80000,
    PS4_PROF_HOST_SLEEP = 0x90000, PS4_PROF_HOST_GETRUSAGE = 0xa0000,
};

extern _Thread_local volatile sig_atomic_t ps4_prof_main, ps4_prof_scope;

static inline uint32_t ps4_profile_enter(uint32_t mask, uint32_t value)
{
    if (!ps4_prof_main) return UINT32_MAX;
    uint32_t previous = (uint32_t)ps4_prof_scope;
    ps4_prof_scope = (sig_atomic_t)((previous & ~mask) | (value & mask));
    return previous;
}

static inline void ps4_profile_leave(uint32_t previous)
{
    if (previous != UINT32_MAX)
        ps4_prof_scope = (sig_atomic_t)previous;
}

typedef struct {
    uint64_t end, span, lock, fence;
    double work_cpu_ms, fence_cpu_ms;
    unsigned pc_count;
    Ps4ProfileSample pcs[PS4_FRAME_PC_CAP];
} Ps4MainFrameTiming;

void ps4_main_frame_snapshot(Ps4MainFrameTiming *out);

/* UINT64_MAX marks unavailable readings. Zero is a valid CPU time. */
static inline double ps4_cpu_ms(uint64_t start, uint64_t end)
{
    return end != UINT64_MAX && end >= start ? (end - start) / 1e6 : -1.0;
}

#endif
