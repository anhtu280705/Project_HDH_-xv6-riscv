#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/kprof.h"
#include "user/user.h"

#define MAXS (NCPU * KPROF_PER_CPU)

static struct kprof_sample smp[MAXS];

struct agg {
  uint64 pc;
  uint32 pid;
  uint32 is_user;
  uint32 count;
};
static struct agg ag[MAXS];

static int
get_status(struct kprof_stat *st)
{
  return kprof(KPROF_CMD_STATUS, st, sizeof(*st));
}

static void
print_status(void)
{
  struct kprof_stat st;

  if (get_status(&st) < 0) {
    printf("kprof: cannot read status\n");
    return;
  }
  printf("kprof status (v%d): %s\n", st.version,
         st.running ? "RUNNING" : "stopped");
  printf("  cpus=%d capacity/cpu=%d\n", st.ncpu, st.per_cpu_cap);
  printf("  observed: kernel=%d user=%d\n", st.kernel_samples,
         st.user_samples);
  printf("  recorded=%d dropped=%d\n", st.recorded, st.dropped);
}

static int
cmp_key(struct kprof_sample *a, struct kprof_sample *b)
{
  if (a->is_user != b->is_user)
    return a->is_user < b->is_user ? -1 : 1;
  if (a->pid != b->pid)
    return a->pid < b->pid ? -1 : 1;
  if (a->pc != b->pc)
    return a->pc < b->pc ? -1 : 1;
  return 0;
}

// Shell sort: no recursion, no libc.
static void
sort_samples(int n)
{
  for (int gap = n / 2; gap > 0; gap /= 2) {
    for (int i = gap; i < n; i++) {
      struct kprof_sample t = smp[i];
      int j = i;
      while (j >= gap && cmp_key(&smp[j - gap], &t) > 0) {
        smp[j] = smp[j - gap];
        j -= gap;
      }
      smp[j] = t;
    }
  }
}

static void
sort_agg(int n)
{
  for (int gap = n / 2; gap > 0; gap /= 2) {
    for (int i = gap; i < n; i++) {
      struct agg t = ag[i];
      int j = i;
      while (j >= gap && ag[j - gap].count < t.count) {
        ag[j] = ag[j - gap];
        j -= gap;
      }
      ag[j] = t;
    }
  }
}

// kprof top [N]: group snapshot by (mode, pid, pc), most frequent first.
// Percentages are shares of *recorded* samples (dropped ones are not in the
// denominator). They estimate where timer ticks landed, not CPU time.
static void
top(int limit)
{
  struct kprof_stat st;
  int n = kprof(KPROF_CMD_READ, smp, MAXS);

  if (n < 0 || get_status(&st) < 0) {
    printf("kprof: cannot read samples\n");
    return;
  }
  if (n == 0) {
    printf("kprof: no samples (start the profiler and run a workload)\n");
    return;
  }

  sort_samples(n);
  int m = 0;
  for (int i = 0; i < n; i++) {
    if (m > 0 && ag[m - 1].pc == smp[i].pc &&
        ag[m - 1].is_user == smp[i].is_user && ag[m - 1].pid == smp[i].pid) {
      ag[m - 1].count++;
    } else {
      ag[m].pc = smp[i].pc;
      ag[m].pid = smp[i].pid;
      ag[m].is_user = smp[i].is_user;
      ag[m].count = 1;
      m++;
    }
  }
  sort_agg(m);

  int ku = 0;
  for (int i = 0; i < n; i++)
    ku += smp[i].is_user;
  printf("kprof top: %d recorded samples (%d kernel, %d user), %d dropped\n", n,
         n - ku, ku, st.dropped);
  printf("  %%=share of recorded samples; kernel PCs are raw addresses until the\n"
         "  symbol table is available; user PCs are never kernel-symbolized.\n");
  printf("   count     %%  mode    pid  pc\n");
  for (int i = 0; i < m && i < limit; i++) {
    int pm = ag[i].count * 1000 / n;
    printf("  %6d %3d.%d  %s %5d  %p\n", ag[i].count, pm / 10, pm % 10,
           ag[i].is_user ? "user  " : "kernel", ag[i].pid,
           (void *)ag[i].pc);
  }
  if (m > limit)
    printf("  ... %d more distinct PCs\n", m - limit);
}

static void
dump(int limit)
{
  int n = kprof(KPROF_CMD_READ, smp, MAXS);

  if (n < 0) {
    printf("kprof: cannot read samples\n");
    return;
  }
  printf("kprof dump: %d samples\n", n);
  for (int i = 0; i < n && i < limit; i++)
    printf("  [%d] cpu=%d pid=%d %s pc=%p\n", i, smp[i].cpu, smp[i].pid,
           smp[i].is_user ? "user" : "kernel", (void *)smp[i].pc);
  if (n > limit)
    printf("  ... %d more\n", n - limit);
}

static int
simple(int cmd, char *ok)
{
  if (kprof(cmd, 0, 0) < 0) {
    printf("kprof: command failed (%s)\n",
           cmd == KPROF_CMD_RESET ? "stop the profiler before reset" : "error");
    return 1;
  }
  printf("kprof: %s\n", ok);
  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("usage: kprof start|stop|reset|status|top [N]|dump [N]\n");
    exit(1);
  }
  if (strcmp(argv[1], "start") == 0)
    exit(simple(KPROF_CMD_START, "sampling started"));
  if (strcmp(argv[1], "stop") == 0)
    exit(simple(KPROF_CMD_STOP, "sampling stopped"));
  if (strcmp(argv[1], "reset") == 0)
    exit(simple(KPROF_CMD_RESET, "data cleared"));
  if (strcmp(argv[1], "status") == 0)
    print_status();
  else if (strcmp(argv[1], "top") == 0)
    top(argc > 2 ? atoi(argv[2]) : 10);
  else if (strcmp(argv[1], "dump") == 0)
    dump(argc > 2 ? atoi(argv[2]) : 20);
  else {
    printf("kprof: unknown command '%s'\n", argv[1]);
    exit(1);
  }
  exit(0);
}
