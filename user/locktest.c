#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD 4
#define NITER  50

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
  printf("=== BAT DAU LOCKTEST (CPUS contention) ===\n");
  test_contention();
  lockstat();
  printf("=== LOCKTEST: [PASS] ===\n");
  exit(0);
}
