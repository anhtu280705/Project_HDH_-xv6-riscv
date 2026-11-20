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
