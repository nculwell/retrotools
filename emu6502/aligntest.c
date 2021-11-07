
#include <stdio.h>
#include "align.h"

int main(int argc, char** argv) {
  for (unsigned alignment = 2; alignment <= 8; alignment *= 2)
    for (unsigned i=0; i < 10; i++) {
      printf("%02X: %02X (align %d)\n", i, nextAligned(i, alignment), alignment);
    }
}

