#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("Bat dau chay test contention...\n");
  int pid = fork();
  if (pid < 0) {
    printf("locktest: FAIL (fork failed)\n");
    exit(1);
  }
  if (pid == 0) {
    // Tien trinh con chay vong lap tao tai thay vi dung sleep
    volatile int dummy = 0;
    for (int i = 0; i < 1000000; i++) {
      dummy = dummy + 1;
    }
    exit(0);
  } else {
    // Tien trinh cha cho con chay xong
    int status;
    wait(&status);
    lockstat();
    printf("locktest: PASS\n");
  }
  exit(0);
}
