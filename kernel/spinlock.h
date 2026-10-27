struct spinlock {
  uint locked; // Is the lock held?

  // For debugging:
  char *name;      // Name of lock.
  struct cpu *cpu; // The cpu holding the lock.

  uint acquisitions;
  uint contended_acquisitions;
  uint failed_attempts;
};
