// vim: et ts=8 sts=2 sw=2

// shared.c
// Author: Nathan Culwell-Kanarek
// Date: 2021-11-05
// I place this code in the public domain.

#include "shared.h"

void Die_impl(const char* file, int line, const char* msg, ...) {
  putchar('\n');
  fprintf(stderr, "[%s:%d] ", file, line);
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  putc('\n', stderr);
  exit(255);
}

void ReadFileWithKnownLength(
    const char* path, Stream* stream, unsigned file_len)
{
  if (file_len > stream->cap)
    Die("File length %d exceeds buffer size %d.", file_len, stream->cap);
  FILE* f = fopen(path, "rb");
  if (!f) {
    Die("Unable to open file: %s", path);
  }
  size_t n_read = fread(stream->buf, 1, file_len, f);
  if (n_read != file_len || ferror(f))
    Die("Error reading file: %s", path);
  // Can't do this check currently because we extract files with trailing zeroes.
  //if (getc(f) != EOF)
  //  Die("File too short: %s", path);
  if (ferror(f))
    Die("Error reading file: %s", path);
  stream->pos = 0;
  stream->len = file_len;
}

void ReadFileWithLengthPrefix(
    const char* path, Stream* stream,
    unsigned bytes_to_skip)
{
  if (stream->cap < bytes_to_skip + 2)
    Die("File buffer is too small.");
  printf("Reading file: %s\n", path);
  FILE* f = fopen(path, "rb");
  if (!f)
    Die("Unable to open file: %s", path);
  size_t offset = 0;
  size_t n_read;
  if (bytes_to_skip > 0) {
    n_read = fread(stream->buf, 1, bytes_to_skip, f);
    if (n_read != bytes_to_skip || ferror(f))
      Die("Error reading file: %s", path);
    offset += n_read;
  }
  n_read = fread(stream->buf + offset, 1, 2, f);
  if (n_read != 2 || ferror(f))
    Die("Error reading file: %s", path);
  unsigned file_len =
    (unsigned)stream->buf[offset]
    | ((unsigned)stream->buf[offset+1] << 8);
  //printf("File len bytes: %02X %02X\n", file_buffer[0], file_buffer[1]);
  offset += 2;
  unsigned after_length_offset = offset;
  if (file_len > stream->cap)
    Die("File's indicated length (%d) too long for available buffer (%d): %s",
        file_len, stream->cap, path);
  printf("(reading file with length: %02x)\n", file_len);
#if 0
  ReadFileWithKnownLength(path, file_buffer, file_len + bytes_to_skip + 2);
#endif
  n_read = fread(stream->buf + offset, 1, file_len, f);
  if (n_read != file_len || ferror(f))
    Die("Error reading file: %s", path);
  offset += n_read;
  stream->pos = after_length_offset;
  stream->len = after_length_offset + file_len;
}

unsigned Read(Stream* s) {
  if (s->pos == s->len)
    Die("Read beyond end of file.");
  return s->buf[s->pos++];
}

unsigned ReadUint16(Stream* s) {
  unsigned lo = Read(s);
  unsigned hi = Read(s);
  return lo | (hi << 8);
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

char* TranslateChar(unsigned c) {
  // TODO: Check translation of string characters
  static char buf[10];
  unsigned unshifted = c & 0x7F;
  if (unshifted < 0x20) {
    sprintf(buf, "<C-%c>", 0x40 + unshifted);
  } else {
    buf[0] = unshifted;
    buf[1] = 0;
  }
  return buf;
}

