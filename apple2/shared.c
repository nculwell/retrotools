// vim: et ts=8 sts=2 sw=2

// shared.c
// Author: Nathan Culwell-Kanarek
// Date: 2021-11-05
// I place this code in the public domain.

#include "shared.h"

void Die_impl(const char* file, int line, const char* msg, ...) {
  fprintf(stderr, "[%s:%d] ", file, line);
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  putc('\n', stderr);
  exit(255);
}

void ReadFileWithKnownLength(
    const char* path, unsigned char* file_buffer, unsigned file_len)
{
  FILE* f = fopen(path, "rb");
  if (!f) {
    Die("Unable to open file: %s", path);
  }
  size_t n_read = fread(file_buffer, 1, file_len, f);
  if (n_read != file_len || ferror(f))
    Die("Error reading file: %s", path);
  //if (getc(f) != EOF)
  //  Die("File too short: %s", path);
  if (ferror(f))
    Die("Error reading file: %s", path);
}

unsigned ReadFileWithLengthPrefix(
    const char* path, unsigned char* file_buffer, unsigned file_buffer_len,
    unsigned bytes_to_skip)
{
  if (file_buffer_len < bytes_to_skip + 2)
    Die("File buffer is too small.");
  printf("Reading file: %s\n", path);
  FILE* f = fopen(path, "rb");
  if (!f)
    Die("Unable to open file: %s", path);
  size_t offset = 0;
  size_t n_read;
  if (bytes_to_skip > 0) {
    n_read = fread(file_buffer, 1, bytes_to_skip, f);
    if (n_read != bytes_to_skip || ferror(f))
      Die("Error reading file: %s", path);
    offset += n_read;
  }
  n_read = fread(file_buffer + offset, 1, 2, f);
  if (n_read != 2 || ferror(f))
    Die("Error reading file: %s", path);
  unsigned file_len =
    (unsigned)file_buffer[offset]
    | ((unsigned)file_buffer[offset+1] << 8);
  //printf("File len bytes: %02X %02X\n", file_buffer[0], file_buffer[1]);
  offset += 2;
  if (file_len > file_buffer_len)
    Die("File's indicated length (%d) too long for available buffer (%d): %s",
        file_len, file_buffer_len, path);
  printf("(reading file with length: %02x)\n", file_len);
#if 0
  ReadFileWithKnownLength(path, file_buffer, file_len + bytes_to_skip + 2);
#endif
  n_read = fread(file_buffer + offset, 1, file_len, f);
  if (n_read != file_len || ferror(f))
    Die("Error reading file: %s", path);
  offset += n_read;
  return offset;
}

void ConcatPath(
    char* path, int path_len, const char* part1, const char* part2, char separator)
{
  int i=0;
  for (; i < path_len-1 && part1[i] != 0; i++) {
    assert(i < path_len);
    path[i] = part1[i];
  }
  if (part1[i] != 0)
    Die("Part 1 of path is too long: %s", part1);
  if (i > 0 && path[i-1] != separator) {
    path[i++] = separator;
  }
  int i2=0;
  for (; i < path_len-1 && part2[i2] != 0; i++, i2++) {
    assert(i < path_len);
    path[i] = part2[i2];
  }
  if (part2[i2] != 0)
    Die("Part 2 of path is too long: %s", part2);
  assert(i < path_len);
  path[i] = 0;
}

