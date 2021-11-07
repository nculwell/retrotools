#ifndef __ALIGN_H__
#define __ALIGN_H__
static inline unsigned nextAligned(unsigned addr, unsigned alignment) {
  return (addr + (alignment-1)) & ~(alignment - 1);
}
#endif
