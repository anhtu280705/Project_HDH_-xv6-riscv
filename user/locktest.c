#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  printf("Bat dau chay test contention...\n");
  
  int pid = fork();
  if (pid < 0) {
    printf("locktest: FAIL (fork failed)\n");
    exit(1);
  }
  
  if (pid == 0) {
    // Tiến trình con chạy vòng lặp tạo tải thay vì dùng sleep
    volatile int dummy = 0;
    for (int i = 0; i < 1000000; i++) {
        dummy = dummy + 1;
    }
    exit(0);
  } else {
    // Tiến trình cha chờ con chạy xong
    int status;
    wait(&status);
    lockstat();
    printf("locktest: PASS\n");
  }
  
  exit(0);
}
