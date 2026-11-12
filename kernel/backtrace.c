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
static int
backtrace_frame_valid(uint64 fp, uint64 stack_bottom, uint64 stack_top)
{
  return fp >= stack_bottom + 16 && fp < stack_top && (fp & 7) == 0;
}

static int
backtrace_link_valid(uint64 fp, uint64 previous_fp, uint64 stack_top)
{
  return previous_fp > fp && previous_fp <= stack_top;
}
// Test frame validation using synthetic addresses.
// No memory is accessed through these addresses.
static int
backtrace_validation_test(void)
{
  uint64 stack_bottom = 0x1000;
  uint64 stack_top = 0x2000;

  // Valid frame.
  if (!backtrace_frame_valid(0x1010, stack_bottom, stack_top))
    return -1;

  // Invalid current frame: below minimum, unaligned, or at stack top.
  if (backtrace_frame_valid(0x1008, stack_bottom, stack_top))
    return -1;

  if (backtrace_frame_valid(0x1011, stack_bottom, stack_top))
    return -1;

  if (backtrace_frame_valid(0x2000, stack_bottom, stack_top))
    return -1;

  // Valid link to the next frame.
  if (!backtrace_link_valid(0x1010, 0x1020, stack_top))
    return -1;

  // Invalid links: same address, lower address, or beyond stack top.
  if (backtrace_link_valid(0x1010, 0x1010, stack_top))
    return -1;

  if (backtrace_link_valid(0x1010, 0x1008, stack_top))
    return -1;

  if (backtrace_link_valid(0x1010, 0x2008, stack_top))
    return -1;

  return 0;
}
// Prototype: walk the current kernel stack and print return addresses.
void
backtrace(void)
{
  uint64 fp = read_fp();
  uint64 stack_bottom = r_sp() & ~(PGSIZE - 1);
  uint64 stack_top = stack_bottom + PGSIZE;
  const char *name;
  uint64 offset;
  printk("backtrace:\n");

  while (backtrace_frame_valid(fp, stack_bottom, stack_top)) {
    uint64 *frame = (uint64 *)fp;
    uint64 previous_fp = frame[-2];
    uint64 return_address = frame[-1];

    if (return_address == 0)
      break;

    if (ksym_lookup(return_address - 1, &name, &offset) == 0) {
      printk("  %s+0x%lx\n", (char *)name, offset);
    } else {
      printk("  ra=%p (unknown)\n", (void *)return_address);
    }

    // Frame pointers should move toward higher stack addresses
    // when walking toward older frames on this RISC-V setup.
    if (!backtrace_link_valid(fp, previous_fp, stack_top))
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
uint64
sys_bttest(void)
{
  backtrace_test();
  if (backtrace_validation_test() < 0)
    printk("backtrace_validation_test: FAIL\n");
  else
    printk("backtrace_validation_test: PASS\n");

  if (ksym_test() < 0)
    printk("ksym_test: FAIL\n");
  else
    printk("ksym_test: PASS\n");

  return 0;
}
