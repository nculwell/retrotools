// vim: et ts=8 sts=2 sw=2

// shared.c
// Author: Nathan Culwell-Kanarek
// Date: 2021-11-05
// I place this code in the public domain.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#define DSK_SIZE 143360
#define PATH_LEN 1024
#define SECTOR_SIZE 256

#define STREAM_BUFFER_SIZE (DSK_SIZE)

#if defined(_WIN32) && !defined(__CYGWIN__)
# define PATH_SEPARATOR '\\'
#else
# define PATH_SEPARATOR '/'
#endif

#define Die(...) Die_impl(__FILE__, __LINE__, __VA_ARGS__)

struct Stream {
  unsigned cap;
  unsigned pos;
  unsigned len;
  unsigned char buf[STREAM_BUFFER_SIZE];
};
typedef struct Stream Stream;

void Die_impl(const char* file, int line, const char* msg, ...) __attribute__((noreturn));
void ReadFileWithKnownLength(const char* path, unsigned char* file_buffer, unsigned file_len);
unsigned ReadFileWithLengthPrefix(const char* path, unsigned char* file_buffer, unsigned file_buffer_len, unsigned bytes_to_skip);
void ConcatPath(char* path, int path_len, const char* part1, const char* part2, char separator);

