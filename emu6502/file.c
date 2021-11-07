
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "em.h"

#define FILE_BLOCK_SIZE SECTOR_SIZE

static inline void bufCheckAlloc(buf_t* buf) {
  if (!buf->data) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
}

buf_t* bufCreate() {
  buf_t* buf = malloc(sizeof(buf_t));
  if (!buf) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  buf->cap = FILE_BLOCK_SIZE;
  buf->len = 0;
  buf->data = malloc(buf->cap);
  bufCheckAlloc(buf);
  return buf;
}

void bufEnsureCap(buf_t* buf, unsigned cap) {
  if (buf->cap < cap) {
    while (buf->cap < cap)
      buf->cap += FILE_BLOCK_SIZE;
    buf->data = realloc(buf->data, buf->cap);
    bufCheckAlloc(buf);
  }
}

void bufEnsureExtraCap(buf_t* buf, unsigned extraCap) {
  bufEnsureCap(buf, buf->len + extraCap);
}

void bufDestroy(buf_t* buf) {
  assert(buf);
  assert(buf->data);
  free(buf->data);
}

void bufAppendChar(buf_t* buf, int c) {
  bufEnsureCap(buf, buf->len+1);
  buf->data[buf->len++] = c;
}

void bufAppend(buf_t* buf, const char* str) {
  while (*str)
    bufAppendChar(buf, *str);
}

// Load a ROM file, which should have the exact size specified.
void loadROM(const char* path, byte_t* loadBuf, size_t size) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Unable to open ROM file: %s\n", path);
    exit(1);
  }
  size_t nRead = fread(loadBuf, 1, size, f);
  if (nRead < size) {
    if (feof(f))
      fprintf(stderr, "ROM file is too small: %s\n", path);
    else
      fprintf(stderr, "Error reading ROM file: %s\n", path);
    exit(1);
  }
  int finalRead = getc(f);
  if (finalRead != EOF) {
    fprintf(stderr, "ROM file is too large: %s\n", path);
    exit(1);
  }
}

buf_t* readFile(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Unable to open file: %s\n", path);
    exit(1);
  }
  buf_t* buf = bufCreate();
  unsigned readSize = FILE_BLOCK_SIZE;
  for (;;) {
    bufEnsureExtraCap(buf, readSize);
    size_t nRead = fread(buf->data + buf->len, 1, readSize, f);
    buf->len += nRead;
    if (nRead < readSize) {
      if (feof(f))
        break;
      fprintf(stderr, "Error reading file: %s\n", path);
      exit(1);
    }
  }
  return buf;
}

