#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_LOCKS 128

struct lockstat g_stats[MAX_LOCKS];
struct lockstat g_s1[MAX_LOCKS];
struct lockstat g_s2[MAX_LOCKS];
struct lockstat g_delta[MAX_LOCKS];

void
sort_stats(struct lockstat *arr, int n, int sort_by_acq)
{
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      int swap = 0;
      if (sort_by_acq) {
        if (arr[j].acquisitions < arr[j + 1].acquisitions)
          swap = 1;
      } else {
        if (arr[j].contended_acquisitions < arr[j + 1].contended_acquisitions)
          swap = 1;
        else if (arr[j].contended_acquisitions ==
                   arr[j + 1].contended_acquisitions &&
                 arr[j].failed_attempts < arr[j + 1].failed_attempts)
          swap = 1;
      }
      if (swap) {
        struct lockstat tmp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = tmp;
      }
    }
  }
}

void
print_pad(char *s, int width)
{
  int len = strlen(s);
  printf("%s", s);
  for (int i = len; i < width; i++)
    printf(" ");
}

void
print_num_pad(int val, int width)
{
  char buf[16];
  int idx = 0;
  if (val == 0) {
    buf[idx++] = '0';
  } else {
    int tmp = val;
    char rev[16];
    int r = 0;
    while (tmp > 0) {
      rev[r++] = '0' + (tmp % 10);
      tmp /= 10;
    }
    while (r > 0)
      buf[idx++] = rev[--r];
  }
  buf[idx] = 0;
  print_pad(buf, width);
}

void
print_table(struct lockstat *arr, int n)
{
  print_pad("LOCK NAME", 18);
  print_pad("ACQUIRES", 14);
  print_pad("CONTENDED", 14);
  printf("FAILED_SPINS\n");
  printf("------------------------------------------------------------\n");
  for (int i = 0; i < n; i++) {
    print_pad(arr[i].name, 18);
    print_num_pad(arr[i].acquisitions, 14);
    print_num_pad(arr[i].contended_acquisitions, 14);
    printf("%d\n", arr[i].failed_attempts);
  }
}

int
main(int argc, char *argv[])
{
  if (argc >= 2 && strcmp(argv[1], "reset") == 0) {
    if (lockstat_ctl(1) < 0) {
      printf("lockstat: reset failed\n");
      exit(1);
    }
    printf("lockstat: counters reset to 0\n");
    exit(0);
  }

  if (argc >= 2 && strcmp(argv[1], "delta") == 0) {
    int ticks = 10;
    if (argc >= 3)
      ticks = atoi(argv[2]) * 10;
    if (ticks <= 0)
      ticks = 10;

    int n1 = lockstat(g_s1, MAX_LOCKS);
    pause(ticks);
    int n2 = lockstat(g_s2, MAX_LOCKS);

    int nd = 0;
    for (int i = 0; i < n2; i++) {
      int acq1 = 0, con1 = 0, fail1 = 0;
      for (int j = 0; j < n1; j++) {
        if (strcmp(g_s2[i].name, g_s1[j].name) == 0) {
          acq1 = g_s1[j].acquisitions;
          con1 = g_s1[j].contended_acquisitions;
          fail1 = g_s1[j].failed_attempts;
          break;
        }
      }
      int da = g_s2[i].acquisitions - acq1;
      int dc = g_s2[i].contended_acquisitions - con1;
      int df = g_s2[i].failed_attempts - fail1;
      if (da > 0 || dc > 0 || df > 0) {
        strcpy(g_delta[nd].name, g_s2[i].name);
        g_delta[nd].acquisitions = da > 0 ? da : 0;
        g_delta[nd].contended_acquisitions = dc > 0 ? dc : 0;
        g_delta[nd].failed_attempts = df > 0 ? df : 0;
        nd++;
      }
    }
    sort_stats(g_delta, nd, 0);
    printf("=== LOCKSTAT DELTA (%d ticks) ===\n", ticks);
    print_table(g_delta, nd);
    exit(0);
  }

  int sort_by_acq = 0;
  if (argc >= 2 && strcmp(argv[1], "--sort=acquires") == 0) {
    sort_by_acq = 1;
  }

  int n = lockstat(g_stats, MAX_LOCKS);
  if (n < 0) {
    printf("lockstat: syscall failed\n");
    exit(1);
  }

  sort_stats(g_stats, n, sort_by_acq);
  print_table(g_stats, n);
  exit(0);
}
