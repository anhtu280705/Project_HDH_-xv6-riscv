// Automated tests for the sampling profiler (kprof). One command, each case
// prints "KPxx PASS|FAIL name"; exit status is non-zero if any case failed.
//
//   $ kproftest            # all cases
//
// Needs a kernel built with KPROF_TEST=1 (default): the cases use the
// TEST_SPIN hook (kernel hot loop with a known PC range) and TEST_SETCAP.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/kprof.h"
#include "user/user.h"

#define MAXS (NCPU * KPROF_PER_CPU)
#define KERNBASE_ 0x80000000UL

static struct kprof_sample smp[MAXS];

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("    check failed: %s (line %d)\n", #c, __LINE__);                \
      ok = 0;                                                                  \
    }                                                                          \
  } while (0)

static int
status(struct kprof_stat *st)
{
  return kprof(KPROF_CMD_STATUS, st, sizeof(*st));
}

static int
reset_all(void)
{
  if (kprof(KPROF_CMD_STOP, 0, 0) < 0 || kprof(KPROF_CMD_RESET, 0, 0) < 0 ||
      kprof(KPROF_CMD_TEST_SETCAP, 0, KPROF_PER_CPU) < 0)
    return -1;
  return 0;
}

// User hot loop with a known PC range. noinline keeps the labels unique.
extern char kp_user_begin[], kp_user_end[];
__attribute__((noinline)) static void
heavy_chunk(volatile uint64 *x)
{
  asm volatile(".globl kp_user_begin\nkp_user_begin:" ::: "memory");
  for (uint64 i = 0; i < (1 << 20); i++)
    *x += i * 3;
  asm volatile(".globl kp_user_end\nkp_user_end:" ::: "memory");
}

static void
spin_user(int ticks)
{
  volatile uint64 x = 0;
  int t0 = uptime();
  while (uptime() - t0 < ticks)
    heavy_chunk(&x);
}

static int
kspin(int ticks, struct kprof_range *r)
{
  struct kprof_range tmp;
  return kprof(KPROF_CMD_TEST_SPIN, r ? r : &tmp, ticks);
}

// KP01: with the profiler off, a workload produces no samples.
static int
kp01(void)
{
  int ok = 1;
  struct kprof_stat st;
  CHECK(reset_all() == 0);
  spin_user(5);
  CHECK(kspin(5, 0) == 0);
  CHECK(status(&st) == 0);
  CHECK(st.running == 0);
  CHECK(st.recorded == 0 && st.kernel_samples == 0 && st.user_samples == 0);
  return ok;
}

// KP02: start -> workload produces samples; bookkeeping is consistent.
static int
kp02(void)
{
  int ok = 1;
  struct kprof_stat st;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kspin(15, 0) == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&st) == 0);
  CHECK(st.running == 0);
  CHECK(st.recorded > 0);
  CHECK(st.kernel_samples + st.user_samples == st.recorded + st.dropped);
  CHECK(st.version == KPROF_VERSION);
  return ok;
}

// KP03: after stop the numbers are frozen even though a workload runs.
static int
kp03(void)
{
  int ok = 1;
  struct kprof_stat a, b;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kspin(5, 0) == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&a) == 0);
  CHECK(kspin(5, 0) == 0);
  spin_user(3);
  CHECK(status(&b) == 0);
  CHECK(a.recorded > 0);
  CHECK(a.recorded == b.recorded && a.dropped == b.dropped);
  CHECK(a.kernel_samples == b.kernel_samples);
  CHECK(a.user_samples == b.user_samples);
  return ok;
}

// KP04: reset is refused while running, clears everything once stopped.
static int
kp04(void)
{
  int ok = 1;
  struct kprof_stat a, b;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kspin(5, 0) == 0);
  CHECK(status(&a) == 0);
  CHECK(kprof(KPROF_CMD_RESET, 0, 0) == -1);
  CHECK(status(&b) == 0);
  CHECK(b.running == 1);
  CHECK(b.recorded >= a.recorded); // the refused reset cleared nothing
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(kprof(KPROF_CMD_RESET, 0, 0) == 0);
  CHECK(status(&b) == 0);
  CHECK(b.recorded == 0 && b.dropped == 0 && b.kernel_samples == 0 &&
        b.user_samples == 0);
  CHECK(kprof(KPROF_CMD_READ, smp, MAXS) == 0);
  return ok;
}

// KP05: repeated start/stop is idempotent and never fails.
static int
kp05(void)
{
  int ok = 1;
  struct kprof_stat st;
  CHECK(reset_all() == 0);
  for (int i = 0; i < 50; i++) {
    CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
    CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
    CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
    CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  }
  CHECK(status(&st) == 0);
  CHECK(st.running == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0); // second start must not double-count
  CHECK(kspin(5, 0) == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&st) == 0);
  CHECK(st.kernel_samples + st.user_samples == st.recorded + st.dropped);
  return ok;
}

// KP06: a kernel hot loop is identified. The spinning process is preempted
// by yield() every tick and may migrate between CPUs, so the oracle is by
// pid, not by CPU: among ticks that hit this process in kernel mode, most
// must fall inside the loop's PC range (idle CPUs run pid 0 and are ignored).
static int
kp06(void)
{
  int ok = 1;
  struct kprof_range r;
  int mine = 0, hit = 0, idle = 0;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kspin(30, &r) == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  int n = kprof(KPROF_CMD_READ, smp, MAXS);
  CHECK(n > 0);
  for (int i = 0; i < n; i++) {
    if (smp[i].is_user)
      continue;
    if (smp[i].pid == 0) {
      idle++;
      continue;
    }
    if (smp[i].pid != getpid())
      continue;
    mine++;
    if (smp[i].pc >= r.begin && smp[i].pc < r.end)
      hit++;
  }
  printf("    kernel loop [%p,%p): %d of %d own kernel ticks (%d idle)\n",
         (void *)r.begin, (void *)r.end, hit, mine, idle);
  CHECK(r.begin != 0 && r.end > r.begin);
  CHECK(hit >= 10);
  CHECK(hit * 100 >= mine * 70);
  return ok;
}

// KP06b: same for a user-space hot loop (user PCs, attributed to our pid).
static int
kp06b(void)
{
  int ok = 1;
  int mine = 0, hit = 0, user = 0;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  spin_user(30);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  int n = kprof(KPROF_CMD_READ, smp, MAXS);
  CHECK(n > 0);
  for (int i = 0; i < n; i++) {
    if (!smp[i].is_user)
      continue;
    user++;
    if (smp[i].pid != getpid())
      continue;
    mine++;
    if (smp[i].pc >= (uint64)kp_user_begin && smp[i].pc < (uint64)kp_user_end)
      hit++;
  }
  printf("    user loop [%p,%p): %d of %d own user ticks (%d user total)\n",
         kp_user_begin, kp_user_end, hit, mine, user);
  CHECK(mine == user); // only this process ran user code
  CHECK(hit >= 10);
  CHECK(hit * 100 >= mine * 70);
  return ok;
}

// KP07: user and kernel PCs are classified consistently (no user PC is ever
// presented as a kernel address and vice versa), CPU ids are valid.
static int
kp07(void)
{
  int ok = 1;
  int nk = 0, nu = 0;
  struct kprof_stat st;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kspin(5, 0) == 0);
  spin_user(5);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&st) == 0);
  int n = kprof(KPROF_CMD_READ, smp, MAXS);
  CHECK(n == (int)st.recorded);
  for (int i = 0; i < n; i++) {
    CHECK(smp[i].cpu < st.ncpu);
    CHECK(smp[i].is_user <= 1);
    if (smp[i].is_user) {
      nu++;
      CHECK(smp[i].pc < KERNBASE_);
      CHECK(smp[i].pid != 0);
    } else {
      nk++;
      CHECK(smp[i].pc >= KERNBASE_);
    }
    if (!ok)
      break;
  }
  CHECK(nk > 0 && nu > 0);
  return ok;
}

// KP08: reading status/snapshots while another process keeps sampling never
// yields torn records or a shrinking sample count.
static int
kp08(void)
{
  int ok = 1;
  struct kprof_stat st;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  int pid = fork();
  if (pid < 0) {
    printf("    fork failed\n");
    kprof(KPROF_CMD_STOP, 0, 0);
    return 0;
  }
  if (pid == 0) {
    int bad = 0;
    uint prev = 0;
    int t0 = uptime();
    while (uptime() - t0 < 25) {
      if (status(&st) < 0 || st.recorded < prev)
        bad = 1;
      prev = st.recorded;
      int n = kprof(KPROF_CMD_READ, smp, MAXS);
      if (n < 0)
        bad = 1;
      for (int i = 0; i < n; i++)
        if (smp[i].pc == 0 || smp[i].cpu >= NCPU || smp[i].is_user > 1)
          bad = 1;
    }
    exit(bad);
  }
  CHECK(kspin(30, 0) == 0);
  int xs = -1;
  CHECK(wait(&xs) == pid);
  CHECK(xs == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&st) == 0);
  CHECK(st.recorded > 0);
  return ok;
}

// KP09: a full buffer drops samples visibly instead of overflowing.
static int
kp09(void)
{
  int ok = 1;
  struct kprof_stat st;
  int per[NCPU];
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_TEST_SETCAP, 0, 0) == -1);
  CHECK(kprof(KPROF_CMD_TEST_SETCAP, 0, KPROF_PER_CPU + 1) == -1);
  CHECK(kprof(KPROF_CMD_TEST_SETCAP, 0, 2) == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kprof(KPROF_CMD_TEST_SETCAP, 0, 4) == -1); // not while running
  CHECK(kspin(15, 0) == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&st) == 0);
  CHECK(st.per_cpu_cap == 2);
  CHECK(st.dropped > 0);
  CHECK(st.recorded <= 2 * st.ncpu);
  CHECK(st.kernel_samples + st.user_samples == st.recorded + st.dropped);
  memset(per, 0, sizeof(per));
  int n = kprof(KPROF_CMD_READ, smp, MAXS);
  CHECK(n == (int)st.recorded);
  for (int i = 0; i < n; i++)
    if (smp[i].cpu < NCPU)
      per[smp[i].cpu]++;
  for (int c = 0; c < NCPU; c++)
    CHECK(per[c] <= 2);
  CHECK(kprof(KPROF_CMD_TEST_SETCAP, 0, KPROF_PER_CPU) == 0);
  return ok;
}

// KP10: bad user pointers, sizes and commands are rejected with -1 and the
// kernel keeps working.
static int
kp10(void)
{
  int ok = 1;
  struct kprof_stat st;
  uint64 heap_end = (uint64)sbrk(0);
  uint64 page_end = (heap_end + 4095) & ~4095UL;

  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  CHECK(kspin(5, 0) == 0);
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  CHECK(status(&st) == 0 && st.recorded > 0);

  // bad destination addresses
  CHECK(kprof(KPROF_CMD_STATUS, (void *)0, sizeof(st)) == -1);
  CHECK(kprof(KPROF_CMD_STATUS, (void *)KERNBASE_, sizeof(st)) == -1);
  CHECK(kprof(KPROF_CMD_STATUS, (void *)(1UL << 38), sizeof(st)) == -1);
  CHECK(kprof(KPROF_CMD_STATUS, (void *)(page_end - 4), sizeof(st)) == -1);
  CHECK(kprof(KPROF_CMD_READ, (void *)KERNBASE_, 4) == -1);
  CHECK(kprof(KPROF_CMD_READ, (void *)(page_end - 4), 4) == -1);
  CHECK(kprof(KPROF_CMD_TEST_SPIN, (void *)KERNBASE_, 1) == -1);

  // bad sizes
  CHECK(kprof(KPROF_CMD_STATUS, &st, sizeof(st) - 1) == -1);
  CHECK(kprof(KPROF_CMD_STATUS, &st, 0) == -1);
  CHECK(kprof(KPROF_CMD_STATUS, &st, -1) == -1);
  CHECK(kprof(KPROF_CMD_READ, smp, 0) == -1);
  CHECK(kprof(KPROF_CMD_READ, smp, -5) == -1);
  CHECK(kprof(KPROF_CMD_TEST_SPIN, smp, -1) == -1);

  // unknown commands
  CHECK(kprof(0, 0, 0) == -1);
  CHECK(kprof(99, 0, 0) == -1);
  CHECK(kprof(-1, 0, 0) == -1);

  // capacity smaller / larger than the data: truncate, never overrun
  CHECK(kprof(KPROF_CMD_READ, smp, 1) == 1);
  CHECK(kprof(KPROF_CMD_READ, smp, 1 << 20) == (int)st.recorded);

  // still alive and consistent
  struct kprof_stat st2;
  CHECK(status(&st2) == 0);
  CHECK(st2.recorded == st.recorded);
  return ok;
}

// KP11: samples carry the pid of the process that was running.
static int
kp11(void)
{
  int ok = 1;
  int total = 0, own = 0;
  CHECK(reset_all() == 0);
  CHECK(kprof(KPROF_CMD_START, 0, 0) == 0);
  int pid = fork();
  if (pid < 0) {
    kprof(KPROF_CMD_STOP, 0, 0);
    return 0;
  }
  if (pid == 0) {
    spin_user(20);
    exit(0);
  }
  CHECK(wait(0) == pid); // parent sleeps: runs no user code meanwhile
  CHECK(kprof(KPROF_CMD_STOP, 0, 0) == 0);
  int n = kprof(KPROF_CMD_READ, smp, MAXS);
  for (int i = 0; i < n; i++) {
    if (!smp[i].is_user)
      continue;
    total++;
    if (smp[i].pid == pid)
      own++;
  }
  printf("    child pid %d: %d of %d user ticks\n", pid, own, total);
  CHECK(own >= 8);
  CHECK(own * 100 >= total * 90);
  return ok;
}

struct tcase {
  char *id;
  char *name;
  int (*fn)(void);
};

static struct tcase cases[] = {
    {"KP01", "off_no_samples", kp01},
    {"KP02", "start_collects", kp02},
    {"KP03", "stop_freezes", kp03},
    {"KP04", "reset_contract", kp04},
    {"KP05", "repeat_start_stop", kp05},
    {"KP06", "kernel_hot_loop", kp06},
    {"KP06b", "user_hot_loop", kp06b},
    {"KP07", "pc_classification", kp07},
    {"KP08", "snapshot_while_sampling", kp08},
    {"KP09", "buffer_overflow_dropped", kp09},
    {"KP10", "bad_args", kp10},
    {"KP11", "pid_attribution", kp11},
};

int
main(int argc, char *argv[])
{
  int n = sizeof(cases) / sizeof(cases[0]);
  int pass = 0;

  for (int i = 0; i < n; i++) {
    if (argc > 1 && strcmp(argv[1], cases[i].id) != 0)
      continue;
    int r = cases[i].fn();
    kprof(KPROF_CMD_STOP, 0, 0);
    printf("%s %s %s\n", cases[i].id, r ? "PASS" : "FAIL", cases[i].name);
    pass += r;
  }
  kprof(KPROF_CMD_STOP, 0, 0);
  kprof(KPROF_CMD_RESET, 0, 0);
  kprof(KPROF_CMD_TEST_SETCAP, 0, KPROF_PER_CPU);
  printf("kproftest: %d/%d PASS\n", pass, argc > 1 ? 1 : n);
  printf("kproftest: DONE\n");
  exit(pass == (argc > 1 ? 1 : n) ? 0 : 1);
}
