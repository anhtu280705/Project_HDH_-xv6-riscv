#ifndef _KPROF_H_
#define _KPROF_H_

#include "types.h"

#define KPROF_CMD_START   1
#define KPROF_CMD_STOP    2
#define KPROF_CMD_RESET   3
#define KPROF_CMD_STATUS  4
#define KPROF_CMD_READ    5

#define KPROF_MAX_SAMPLES 1024

struct kprof_stat {
  uint32 is_running;
  uint32 total_samples;
  uint32 kernel_samples;
  uint32 user_samples;
  uint32 dropped_samples;
  uint32 total_ticks;
};

struct kprof_sample {
  uint64 pc;
  uint32 is_user;
  uint32 cpu;
};

#endif
