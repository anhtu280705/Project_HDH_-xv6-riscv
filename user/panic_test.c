#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  panic_test();
  exit(0);
}
