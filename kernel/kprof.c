#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "kprof.h"

// Per-CPU sample buffer. Written only by its own CPU from the timer
// interrupt (interrupts are off there), read by any CPU in process context.
//  - entries [0, count) are complete and immutable until reset;
//  - `count` is published with release and read with acquire;
//  - the counters have a single writer, readers tolerate being slightly
//    stale while profiling runs and are exact once stopped.
struct kprof_cpu {
  struct kprof_sample buf[KPROF_PER_CPU];
  uint32 count;
  uint32 kernel_obs;
  uint32 user_obs;
  uint32 dropped;
  int busy; // 1 while this CPU is inside kprof_sample()
} __attribute__((aligned(64)));

static struct kprof_cpu kcpu[NCPU];
static int kprof_running;
static uint32 kprof_limit = KPROF_PER_CPU;

// Serializes control operations (start/stop/reset/read/status) between
// processes. It is a sleeplock because holding it across copyout is fine,
// and it is never taken in interrupt context.
static struct sleeplock kprof_ctl;

void
kprofinit(void)
{
  initsleeplock(&kprof_ctl, "kprof");
}

// Called from the timer interrupt path (trap.c) with interrupts off.
// Bounded work, no lock, no allocation, no printing, no symbol lookup.
void
kprof_sample(uint64 pc, int is_user)
{
  struct kprof_cpu *c = &kcpu[cpuid()];

  // Announce we are inside before looking at kprof_running so that
  // kprof_stop() can wait for us (store-load ordering needs seq_cst).
  __atomic_store_n(&c->busy, 1, __ATOMIC_SEQ_CST);
  if (__atomic_load_n(&kprof_running, __ATOMIC_SEQ_CST)) {
    struct proc *p = myproc();
    uint32 idx = c->count;

    if (is_user)
      __atomic_store_n(&c->user_obs, c->user_obs + 1, __ATOMIC_RELAXED);
    else
      __atomic_store_n(&c->kernel_obs, c->kernel_obs + 1, __ATOMIC_RELAXED);

    if (idx < __atomic_load_n(&kprof_limit, __ATOMIC_RELAXED)) {
      c->buf[idx].pc = pc;
      c->buf[idx].pid = p ? p->pid : 0;
      c->buf[idx].cpu = cpuid();
      c->buf[idx].is_user = is_user ? 1 : 0;
      __atomic_store_n(&c->count, idx + 1, __ATOMIC_RELEASE);
    } else {
      __atomic_store_n(&c->dropped, c->dropped + 1, __ATOMIC_RELAXED);
    }
  }
  __atomic_store_n(&c->busy, 0, __ATOMIC_RELEASE);
}

// Stop sampling and wait until no CPU is still inside kprof_sample().
// After this returns the buffers are quiescent. Caller holds kprof_ctl.
static void
kprof_stop(void)
{
  __atomic_store_n(&kprof_running, 0, __ATOMIC_SEQ_CST);
  for (int i = 0; i < NCPU; i++)
    while (__atomic_load_n(&kcpu[i].busy, __ATOMIC_ACQUIRE))
      ;
}

static void
kprof_status(struct kprof_stat *st)
{
  memset(st, 0, sizeof(*st));
  st->version = KPROF_VERSION;
  st->running = __atomic_load_n(&kprof_running, __ATOMIC_ACQUIRE);
  st->ncpu = NCPU;
  st->per_cpu_cap = __atomic_load_n(&kprof_limit, __ATOMIC_RELAXED);
  for (int i = 0; i < NCPU; i++) {
    struct kprof_cpu *c = &kcpu[i];
    st->recorded += __atomic_load_n(&c->count, __ATOMIC_ACQUIRE);
    st->kernel_samples += __atomic_load_n(&c->kernel_obs, __ATOMIC_RELAXED);
    st->user_samples += __atomic_load_n(&c->user_obs, __ATOMIC_RELAXED);
    st->dropped += __atomic_load_n(&c->dropped, __ATOMIC_RELAXED);
  }
}

#ifdef KPROF_TEST_HOOKS
// Controlled kernel hot loop with interrupts enabled (we are in a syscall),
// so the timer can interrupt it in supervisor mode. The asm labels bound the
// loop's PCs; noinline keeps the labels unique.
__attribute__((noinline)) static void
kprof_spin(uint64 dur)
{
  volatile uint64 x = 0;
  uint64 t0 = r_time();

  asm volatile(".globl kprof_spin_begin\nkprof_spin_begin:" ::: "memory");
  while (r_time() - t0 < dur)
    x = x + 1;
  asm volatile(".globl kprof_spin_end\nkprof_spin_end:" ::: "memory");
}
extern char kprof_spin_begin[], kprof_spin_end[];
#endif

// Syscall backend. Returns >= 0 on success, -1 on any error.
// buf is a user address; every access goes through copyout, so a bad
// pointer or size is reported as -1.
int
kprof_control(int cmd, uint64 buf, int cap)
{
  struct proc *p = myproc();
  int ret = -1;

#ifdef KPROF_TEST_HOOKS
  if (cmd == KPROF_CMD_TEST_SPIN) {
    struct kprof_range r;
    if (cap < 0 || cap > 600)
      return -1;
    r.begin = (uint64)kprof_spin_begin;
    r.end = (uint64)kprof_spin_end;
    if (copyout(p->pagetable, p->sz, buf, (char *)&r, sizeof(r)) < 0)
      return -1;
    kprof_spin((uint64)cap * 1000000);
    return 0;
  }
#endif

  acquiresleep(&kprof_ctl);
  switch (cmd) {
  case KPROF_CMD_START:
    __atomic_store_n(&kprof_running, 1, __ATOMIC_SEQ_CST);
    ret = 0;
    break;

  case KPROF_CMD_STOP:
    kprof_stop();
    ret = 0;
    break;

  case KPROF_CMD_RESET:
    // Resetting while a CPU may still write would race; require stop first.
    if (__atomic_load_n(&kprof_running, __ATOMIC_ACQUIRE))
      break;
    for (int i = 0; i < NCPU; i++) {
      kcpu[i].count = 0;
      kcpu[i].kernel_obs = 0;
      kcpu[i].user_obs = 0;
      kcpu[i].dropped = 0;
    }
    ret = 0;
    break;

  case KPROF_CMD_STATUS: {
    struct kprof_stat st;
    if (cap < (int)sizeof(st))
      break;
    kprof_status(&st);
    if (copyout(p->pagetable, p->sz, buf, (char *)&st, sizeof(st)) < 0)
      break;
    ret = 0;
    break;
  }

  case KPROF_CMD_READ: {
    // Copies up to cap samples (CPU 0 first). While running the result is
    // a prefix of every CPU's buffer: complete entries only.
    int done = 0;
    if (cap <= 0)
      break;
    ret = 0;
    for (int i = 0; i < NCPU && done < cap; i++) {
      uint32 n = __atomic_load_n(&kcpu[i].count, __ATOMIC_ACQUIRE);
      if (n > (uint32)(cap - done))
        n = cap - done;
      if (n == 0)
        continue;
      if (copyout(p->pagetable, p->sz,
                  buf + (uint64)done * sizeof(struct kprof_sample),
                  (char *)kcpu[i].buf, n * sizeof(struct kprof_sample)) < 0) {
        ret = -1;
        break;
      }
      done += n;
    }
    if (ret == 0)
      ret = done;
    break;
  }

#ifdef KPROF_TEST_HOOKS
  case KPROF_CMD_TEST_SETCAP:
    if (cap < 1 || cap > KPROF_PER_CPU)
      break;
    if (__atomic_load_n(&kprof_running, __ATOMIC_ACQUIRE))
      break;
    __atomic_store_n(&kprof_limit, cap, __ATOMIC_RELAXED);
    ret = 0;
    break;
#endif

  default:
    break;
  }
  releasesleep(&kprof_ctl);
  return ret;
}
