#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/kprof.h"
#include "user/user.h"

static struct kprof_sample sample_buf[KPROF_MAX_SAMPLES];

void
print_status(void)
{
  struct kprof_stat st;
  if (kprof(KPROF_CMD_STATUS, &st) < 0) {
    printf("kprof: failed to get status\n");
    return;
  }
  printf("--- KPROF STATUS ---\n");
  printf("  running:         %d\n", st.is_running);
  printf("  total samples:   %d / %d\n", st.total_samples, KPROF_MAX_SAMPLES);
  printf("  kernel samples:  %d\n", st.kernel_samples);
  printf("  user samples:    %d\n", st.user_samples);
  printf("  dropped samples: %d\n", st.dropped_samples);
  printf("  total ticks:     %d\n", st.total_ticks);
}

void
dump_samples(void)
{
  int n = kprof(KPROF_CMD_READ, sample_buf);
  if (n < 0) {
    printf("kprof: failed to read samples\n");
    return;
  }
  printf("--- KPROF SAMPLES DUMP (%d recorded) ---\n", n);
  int limit = n > 20 ? 20 : n;
  for (int i = 0; i < limit; i++) {
    printf("  [%d] cpu=%d %s pc=%p\n", i, sample_buf[i].cpu,
           sample_buf[i].is_user ? "USER  " : "KERNEL",
           (void *)sample_buf[i].pc);
  }
  if (n > limit) {
    printf("  ... (%d more samples omitted)\n", n - limit);
  }
}

void
run_test(void)
{
  printf("kprof: bat dau test sampling profiler...\n");

  // 1. Reset va kiem tra trang thai ban dau
  if (kprof(KPROF_CMD_RESET, 0) < 0) {
    printf("kprof: FAIL (reset failed)\n");
    return;
  }

  struct kprof_stat st;
  kprof(KPROF_CMD_STATUS, &st);
  if (st.total_samples != 0 || st.is_running != 0) {
    printf("kprof: FAIL (initial state not clean)\n");
    return;
  }

  // 2. Bat profiler
  if (kprof(KPROF_CMD_START, 0) < 0) {
    printf("kprof: FAIL (start failed)\n");
    return;
  }

  // 3. Chay CPU-heavy workload kiem soat de sinh ra cac ngat timer
  printf("kprof: dang chay workload tinh toan de thu thap mau timer...\n");
  volatile uint64 sum = 0;
  for (uint64 i = 0; i < 50000000ULL; i++) {
    sum += (i ^ (sum + 1));
  }

  // 4. Dung profiler
  if (kprof(KPROF_CMD_STOP, 0) < 0) {
    printf("kprof: FAIL (stop failed)\n");
    return;
  }

  // 5. Lay thong ke
  kprof(KPROF_CMD_STATUS, &st);
  print_status();

  if (st.total_ticks > 0 && (st.kernel_samples + st.user_samples > 0)) {
    printf("kprof: PASS (thu thap thanh cong %d mau tu ngat timer)\n",
           st.kernel_samples + st.user_samples);
  } else {
    printf("kprof: WARN (chua co mau nao duoc thu thap)\n");
  }

  dump_samples();
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Su dung: kprof [start|stop|reset|status|dump|test]\n");
    exit(1);
  }

  if (strcmp(argv[1], "start") == 0) {
    if (kprof(KPROF_CMD_START, 0) < 0) {
      printf("kprof: start failed\n");
      exit(1);
    }
    printf("kprof: da bat lay mau timer\n");
  } else if (strcmp(argv[1], "stop") == 0) {
    if (kprof(KPROF_CMD_STOP, 0) < 0) {
      printf("kprof: stop failed\n");
      exit(1);
    }
    printf("kprof: da dung lay mau timer\n");
  } else if (strcmp(argv[1], "reset") == 0) {
    if (kprof(KPROF_CMD_RESET, 0) < 0) {
      printf("kprof: reset failed\n");
      exit(1);
    }
    printf("kprof: da reset toan bo bo dem mau\n");
  } else if (strcmp(argv[1], "status") == 0) {
    print_status();
  } else if (strcmp(argv[1], "dump") == 0) {
    dump_samples();
  } else if (strcmp(argv[1], "test") == 0) {
    run_test();
  } else {
    printf("kprof: lenh khong hop le '%s'\n", argv[1]);
    exit(1);
  }

  exit(0);
}
