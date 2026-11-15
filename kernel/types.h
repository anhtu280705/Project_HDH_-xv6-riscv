typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

struct lockstat {
  char name[16];
  uint acquisitions;
  uint contended_acquisitions;
  uint failed_attempts;
};
