
#include "types.h"
#include "ksym.h"

extern struct ksym ksym_table[];
extern uint64 ksym_count;

int
ksym_lookup(uint64 addr, const char **name, uint64 *offset)
{
  uint64 left = 0;
  uint64 right;

  if (name)
    *name = 0;

  if (offset)
    *offset = 0;

  if (ksym_count == 0)
    return -1;

  right = ksym_count - 1;

  while (left <= right) {
    uint64 mid = left + (right - left) / 2;
    struct ksym *symbol = &ksym_table[mid];

    if (addr < symbol->addr) {
      if (mid == 0)
        break;
      right = mid - 1;
    } else if (addr - symbol->addr >= symbol->size) {
      left = mid + 1;
    } else {
      if (name)
        *name = symbol->name;

      if (offset)
        *offset = addr - symbol->addr;

      return 0;
    }
  }

  return -1;
}
// Test symbol lookup using a known kernel function address.
int
ksym_test(void)
{
  const char *name;
  uint64 offset;

  // Test an address inside backtrace_test.
  if (ksym_lookup(0x80000f50, &name, &offset) < 0)
    return -1;

  if (name == 0)
    return -1;

  if (name[0] != 'b' || name[1] != 'a' ||
    name[2] != 'c' || name[3] != 'k' ||
    name[4] != 't' || name[5] != 'r' ||
    name[6] != 'a' || name[7] != 'c' ||
    name[8] != 'e' || name[9] != '_' ||
    name[10] != 't' || name[11] != 'e' ||
    name[12] != 's' || name[13] != 't' ||
    name[14] != '\0')
  return -1;

  // Test an address outside all function symbols.
  if (ksym_lookup(0, &name, &offset) == 0)
    return -1;

  return 0;
}
