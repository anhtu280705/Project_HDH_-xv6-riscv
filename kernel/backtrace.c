#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// Read the current RISC-V frame pointer (s0).
static uint64
read_fp(void)
{
  uint64 fp;
  asm volatile("mv %0, s0" : "=r"(fp));
  return fp;
}

// Prototype: walk the current kernel stack and print return addresses.
void
backtrace(void)
{
  uint64 fp = read_fp();
  uint64 stack_bottom = r_sp() & ~(PGSIZE - 1);
  uint64 stack_top = stack_bottom + PGSIZE;

  printk("backtrace:\n");

  while (fp >= stack_bottom + 16 && fp < stack_top && (fp & 7) == 0) {
    uint64 *frame = (uint64 *)fp;
    uint64 previous_fp = frame[-2];
    uint64 return_address = frame[-1];

    if (return_address == 0)
      break;

    printk("  ra=%p\n", (void *)return_address);

    // Frame pointers should move toward higher stack addresses
    // when walking toward older frames on this RISC-V setup.
    if (previous_fp <= fp || previous_fp > stack_top)
      break;

    fp = previous_fp;
  }
}

// Controlled call chain for the first prototype test.
__attribute__((noinline)) static void
backtrace_level2(void)
{
  backtrace();
}
__attribute__((noinline)) static void
backtrace_level1(void)
{
  backtrace_level2();
}

void
backtrace_test(void)
{
  backtrace_level1();
}
