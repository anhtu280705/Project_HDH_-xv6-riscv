// Timer-driven sampling profiler: ABI shared by kernel and user space.
// See docs/kprof.md for the state/snapshot contract.
#ifndef _KPROF_H_
#define _KPROF_H_

#include "types.h"

#define KPROF_VERSION 1

// kprof(cmd, buf, cap) commands.
#define KPROF_CMD_START  1 // start sampling (idempotent), returns 0
#define KPROF_CMD_STOP   2 // stop sampling (idempotent), returns 0
#define KPROF_CMD_RESET  3 // clear all data; -1 if still running
#define KPROF_CMD_STATUS 4 // buf: struct kprof_stat, cap: sizeof(buf)
#define KPROF_CMD_READ   5 // buf: struct kprof_sample[cap], returns count

// Test-only hooks, compiled in when the kernel is built with KPROF_TEST=1
// (default for the course build, see Makefile). They never panic the kernel.
#define KPROF_CMD_TEST_SPIN   6 // buf: struct kprof_range, cap: ticks of 0.1s
#define KPROF_CMD_TEST_SETCAP 7 // cap: per-CPU capacity limit, 1..KPROF_PER_CPU

// Samples stored per CPU. Each CPU owns one buffer, so the timer path needs
// no lock. When a buffer is full further samples are counted in `dropped`.
#define KPROF_PER_CPU 256

struct kprof_stat {
  uint32 version;
  uint32 running;
  uint32 ncpu;         // CPUs that may record samples
  uint32 per_cpu_cap;  // effective per-CPU capacity
  uint32 recorded;     // samples stored in buffers
  uint32 kernel_samples; // timer ticks observed in kernel mode while running
  uint32 user_samples;   // timer ticks observed in user mode while running
  uint32 dropped;      // observed ticks that did not fit (recorded + dropped
                       // == kernel_samples + user_samples when stopped)
};

struct kprof_sample {
  uint64 pc;   // interrupted PC. is_user: user VA of process `pid`, never a
               // kernel symbol address. !is_user: kernel text address.
  uint32 pid;  // process running at the tick (0: scheduler / no process)
  uint16 cpu;
  uint16 is_user;
};

struct kprof_range {
  uint64 begin;
  uint64 end;
};

#endif
