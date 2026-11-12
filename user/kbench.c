// Fixed-work benchmark for measuring profiler overhead.
//   kbench cpu <M>   M million iterations of a user-space compute loop
//   kbench sys <K>   K thousand getpid() syscalls
// Prints "kbench: <mode> <arg> ticks=<elapsed>" (1 tick ~ 0.1 s, uptime()).
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("usage: kbench cpu <millions>|sys <thousands>\n");
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
  } else {
    printf("kbench: bad mode\n");
    exit(1);
  }
  printf("kbench: %s %d ticks=%d\n", argv[1], atoi(argv[2]), uptime() - t0);
  exit(0);
}

