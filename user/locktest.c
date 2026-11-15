#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD    4
#define NITER     50
#define MAX_LOCKS 64

struct lockstat stats[MAX_LOCKS];

void
test_contention()
{
  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("locktest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      char buf[2];
      for (int j = 0; j < NITER; j++) {
        int p[2];
        if (pipe(p) >= 0) {
          write(p[1], "ok", 2);
          read(p[0], buf, 2);
          close(p[0]);
          close(p[1]);
        }
      }
      exit(0);
    }
  }

  for (int i = 0; i < NCHILD; i++) {
    wait(0);
  }
}

int
main(int argc, char *argv[])
{
  printf("=== BAT DAU LOCKTEST (WITH ORACLES) ===\n");

  // Oracle 1: Kiem tra reset (LS01)
  if (lockstat_ctl(1) != 0) {
    printf("[FAIL] LS01: lockstat_ctl(RESET) failed\n");
    exit(1);
  }
  printf("[PASS] LS01: Reset counters succeeded\n");

  // Oracle 2: Kiem tra tham so loi (LS02)
  if (lockstat_ctl(999) != -1) {
    printf("[FAIL] LS02: Invalid ctl command did not return -1\n");
    exit(1);
  }
  printf("[PASS] LS02: Invalid ctl command properly rejected\n");

  // Oracle 3: Tao tranh chap va kiem chung metrics (LS03)
  test_contention();
  int n = lockstat(stats, MAX_LOCKS);
  if (n <= 0) {
    printf("[FAIL] LS03: No locks exported\n");
    exit(1);
  }

  int total_acq = 0;
  int total_contended = 0;
  for (int i = 0; i < n; i++) {
    total_acq += stats[i].acquisitions;
    total_contended += stats[i].contended_acquisitions;
  }

  if (total_acq <= 0) {
    printf("[FAIL] LS03: total_acq == 0\n");
    exit(1);
  }
  printf("[PASS] LS03: Exported %d locks (total_acq=%d, contended=%d)\n", n,
         total_acq, total_contended);

  printf("=== LOCKTEST: [PASS] ===\n");
  exit(0);
}
