// Fixed-work benchmark for measuring profiler overhead and hot-loop demos.
//   kbench cpu <M>      M million iterations of a user-space compute loop
//   kbench sys <K>      K thousand getpid() syscalls
//   kbench io <N>       N create/write/read/unlink file I/O cycles
//   kbench kspin <T>    T timer ticks in kernel spin loop (with interrupts on)
// Prints "kbench: <mode> <arg> ticks=<elapsed>" (1 tick ~ 0.1 s, uptime()).
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#ifndef STOCK_XV6
#include "kernel/kprof.h"
#endif
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("usage: kbench cpu <millions>|sys <thousands>|io <iters>|kspin "
           "<ticks>\n");
    exit(1);
  }
  uint64 n = (uint64)atoi(argv[2]);
  volatile uint64 x = 0;
  int t0 = uptime();

  if (strcmp(argv[1], "cpu") == 0) {
    n *= 1000000UL;
    for (uint64 i = 0; i < n; i++)
      x += i * 3 + (x >> 7);
  } else if (strcmp(argv[1], "sys") == 0) {
    n *= 1000UL;
    for (uint64 i = 0; i < n; i++)
      x += getpid();
  } else if (strcmp(argv[1], "io") == 0) {
    char buf[64];
    memset(buf, 'k', sizeof(buf));
    for (uint64 i = 0; i < n; i++) {
      int fd = open("kbench.tmp", O_CREATE | O_WRONLY | O_TRUNC);
      if (fd < 0) {
        printf("kbench: io open write failed\n");
        exit(1);
      }
      if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        close(fd);
        printf("kbench: io write failed\n");
        exit(1);
      }
      close(fd);
      fd = open("kbench.tmp", O_RDONLY);
      if (fd < 0) {
        printf("kbench: io open read failed\n");
        exit(1);
      }
      if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
        close(fd);
        printf("kbench: io read failed\n");
        exit(1);
      }
      close(fd);
      unlink("kbench.tmp");
    }
#ifndef STOCK_XV6
  } else if (strcmp(argv[1], "kspin") == 0) {
    struct kprof_range r;
    if (kprof(KPROF_CMD_TEST_SPIN, &r, (int)n) < 0) {
      printf("kbench: kspin failed (requires KPROF_TEST=1)\n");
      exit(1);
    }
#endif
  } else {
    printf("kbench: bad mode\n");
    exit(1);
  }
  printf("kbench: %s %d ticks=%d\n", argv[1], atoi(argv[2]), uptime() - t0);
  exit(0);
}
