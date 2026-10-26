#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "kprof.h"

static struct spinlock kprof_lock;
static volatile int kprof_running = 0;

static volatile uint32 sample_count = 0;
static volatile uint32 kernel_samples = 0;
static volatile uint32 user_samples = 0;
static volatile uint32 dropped_samples = 0;
static volatile uint32 total_ticks = 0;

static struct kprof_sample samples[KPROF_MAX_SAMPLES];

void
kprofinit(void)
{
  initlock(&kprof_lock, "kprof");
  kprof_running = 0;
  sample_count = 0;
  kernel_samples = 0;
  user_samples = 0;
  dropped_samples = 0;
  total_ticks = 0;
}

// Called from timer interrupt in trap.c.
// Must be non-blocking, lock-free, and fast.
void
kprof_sample(uint64 pc, int is_user)
{
  if (__atomic_load_n(&kprof_running, __ATOMIC_RELAXED) == 0)
    return;

  __atomic_fetch_add(&total_ticks, 1, __ATOMIC_RELAXED);

  if (is_user)
    __atomic_fetch_add(&user_samples, 1, __ATOMIC_RELAXED);
  else
    __atomic_fetch_add(&kernel_samples, 1, __ATOMIC_RELAXED);

  uint32 idx = __atomic_fetch_add(&sample_count, 1, __ATOMIC_RELAXED);
  if (idx < KPROF_MAX_SAMPLES) {
    samples[idx].pc = pc;
    samples[idx].is_user = is_user ? 1 : 0;
    samples[idx].cpu = cpuid();
  } else {
    __atomic_fetch_add(&dropped_samples, 1, __ATOMIC_RELAXED);
  }
}

int
kprof_control(int cmd, uint64 uarg)
{
  struct proc *p = myproc();

  switch (cmd) {
  case KPROF_CMD_START:
    acquire(&kprof_lock);
    __atomic_store_n(&kprof_running, 1, __ATOMIC_RELEASE);
    release(&kprof_lock);
    return 0;

  case KPROF_CMD_STOP:
    acquire(&kprof_lock);
    __atomic_store_n(&kprof_running, 0, __ATOMIC_RELEASE);
    release(&kprof_lock);
    return 0;

  case KPROF_CMD_RESET:
    acquire(&kprof_lock);
    __atomic_store_n(&kprof_running, 0, __ATOMIC_RELEASE);
    sample_count = 0;
    kernel_samples = 0;
    user_samples = 0;
    dropped_samples = 0;
    total_ticks = 0;
    release(&kprof_lock);
    return 0;

  case KPROF_CMD_STATUS: {
    if (uarg == 0)
      return -1;
    struct kprof_stat st;
    acquire(&kprof_lock);
    st.is_running = __atomic_load_n(&kprof_running, __ATOMIC_ACQUIRE);
    uint32 sc = sample_count;
    st.total_samples = (sc > KPROF_MAX_SAMPLES) ? KPROF_MAX_SAMPLES : sc;
    st.kernel_samples = kernel_samples;
    st.user_samples = user_samples;
    st.dropped_samples = dropped_samples;
    st.total_ticks = total_ticks;
    release(&kprof_lock);

    if (copyout(p->pagetable, p->sz, uarg, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }

  case KPROF_CMD_READ: {
    if (uarg == 0)
      return -1;
    acquire(&kprof_lock);
    uint32 count = sample_count;
    if (count > KPROF_MAX_SAMPLES)
      count = KPROF_MAX_SAMPLES;
    uint32 bytes = count * sizeof(struct kprof_sample);
    release(&kprof_lock);

    if (bytes > 0) {
      if (copyout(p->pagetable, p->sz, uarg, (char *)samples, bytes) < 0)
        return -1;
    }
    return count;
  }

  default:
    return -1;
  }
}
