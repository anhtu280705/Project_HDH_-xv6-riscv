#ifndef KSYM_H
#define KSYM_H

#include "types.h"

// Information about one kernel function symbol.
struct ksym {
  uint64 addr;
  uint64 size;
  const char *name;
};

// Find the function symbol containing the given address.
// Return 0 if found, -1 if not found.
int ksym_lookup(uint64 addr, const char **name, uint64 *offset);

#endif
