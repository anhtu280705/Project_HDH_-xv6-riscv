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

static void
print_pad(int val, int width)
{
  int len = 0;
  int v = val;
  if (v <= 0)
    len = 1;
  while (v > 0) {
    len++;
    v /= 10;
  }
  while (len < width) {
    printf(" ");
    len++;
  }
  printf("%d", val);
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
  printf("  %%=share of recorded samples; kernel PCs resolved via ksym_lookup;\n"
         "  user PCs are never kernel-symbolized.\n");
  printf("  count      %%  mode       pid  pc                  symbol\n");
  for (int i = 0; i < m && i < limit; i++) {
    int pm = ag[i].count * 1000 / n;
    printf(" ");
    print_pad(ag[i].count, 6);
    printf(" ");
    print_pad(pm / 10, 3);
    printf(".%d%%  ", pm % 10);
    printf("%s  ", ag[i].is_user ? "user  " : "kernel");
    print_pad(ag[i].pid, 5);
    printf("  %p  ", (void *)ag[i].pc);
    if (ag[i].is_user) {
      printf("<user>\n");
    } else {
      struct kprof_sym sym;
      memset(&sym, 0, sizeof(sym));
      sym.pc = ag[i].pc;
      if (kprof(KPROF_CMD_SYM, &sym, sizeof(sym)) == 0 && sym.found)
        printf("%s+0x%x\n", sym.name, (uint32)sym.offset);
      else
        printf("<unknown>\n");
    }
  }
  if (m > limit)
    printf("  ... %d more distinct PCs\n", m - limit);
}

struct fn_agg {
  char name[32];
  uint32 is_user;
  uint32 count;
  uint32 distinct_pcs;
};
static struct fn_agg fn_ag[MAXS];

// kprof fn [N]: group samples by resolved function symbol (or <user> for
// user-space samples) across all offsets and PIDs, most frequent function first.
static void
fn_top(int limit)
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

  int nf = 0;
  for (int i = 0; i < m; i++) {
    char sym_name[32];
    memset(sym_name, 0, sizeof(sym_name));
    if (ag[i].is_user) {
      strcpy(sym_name, "<user>");
    } else {
      struct kprof_sym sym;
      memset(&sym, 0, sizeof(sym));
      sym.pc = ag[i].pc;
      if (kprof(KPROF_CMD_SYM, &sym, sizeof(sym)) == 0 && sym.found)
        strcpy(sym_name, sym.name);
      else
        strcpy(sym_name, "<unknown>");
    }

    int found_idx = -1;
    for (int j = 0; j < nf; j++) {
      if (fn_ag[j].is_user == ag[i].is_user &&
          strcmp(fn_ag[j].name, sym_name) == 0) {
        found_idx = j;
        break;
      }
    }
    if (found_idx >= 0) {
      fn_ag[found_idx].count += ag[i].count;
      fn_ag[found_idx].distinct_pcs++;
    } else {
      strcpy(fn_ag[nf].name, sym_name);
      fn_ag[nf].is_user = ag[i].is_user;
      fn_ag[nf].count = ag[i].count;
      fn_ag[nf].distinct_pcs = 1;
      nf++;
    }
  }

  for (int gap = nf / 2; gap > 0; gap /= 2) {
    for (int i = gap; i < nf; i++) {
      struct fn_agg t = fn_ag[i];
      int j = i;
      while (j >= gap && fn_ag[j - gap].count < t.count) {
        fn_ag[j] = fn_ag[j - gap];
        j -= gap;
      }
      fn_ag[j] = t;
    }
  }

  int ku = 0;
  for (int i = 0; i < n; i++)
    ku += smp[i].is_user;
  printf("kprof fn: %d recorded samples (%d kernel, %d user), %d functions\n",
         n, n - ku, ku, nf);
  printf("  count      %%  mode     pcs  function\n");
  for (int i = 0; i < nf && i < limit; i++) {
    int pm = fn_ag[i].count * 1000 / n;
    printf(" ");
    print_pad(fn_ag[i].count, 6);
    printf(" ");
    print_pad(pm / 10, 3);
    printf(".%d%%  ", pm % 10);
    printf("%s  ", fn_ag[i].is_user ? "user  " : "kernel");
    print_pad(fn_ag[i].distinct_pcs, 4);
    printf("  %s\n", fn_ag[i].name);
  }
  if (nf > limit)
    printf("  ... %d more functions\n", nf - limit);
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
  for (int i = 0; i < n && i < limit; i++) {
    printf("  [%d] cpu=%d pid=%d %s pc=%p ", i, smp[i].cpu, smp[i].pid,
           smp[i].is_user ? "user  " : "kernel", (void *)smp[i].pc);
    if (smp[i].is_user) {
      printf("<user>\n");
    } else {
      struct kprof_sym sym;
      memset(&sym, 0, sizeof(sym));
      sym.pc = smp[i].pc;
      if (kprof(KPROF_CMD_SYM, &sym, sizeof(sym)) == 0 && sym.found)
        printf("%s+0x%x\n", sym.name, (uint32)sym.offset);
      else
        printf("<unknown>\n");
    }
  }
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
    printf("usage: kprof start|stop|reset|status|top [N]|fn [N]|dump [N]\n");
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
  else if (strcmp(argv[1], "fn") == 0)
    fn_top(argc > 2 ? atoi(argv[2]) : 10);
  else if (strcmp(argv[1], "dump") == 0)
    dump(argc > 2 ? atoi(argv[2]) : 20);
  else {
    printf("kprof: unknown command '%s'\n", argv[1]);
    exit(1);
  }
  exit(0);
}
