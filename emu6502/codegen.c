// vim: et ts=8 sts=2 sw=2 cindent

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>

const char* USAGE =
"USAGE: codegen instruction_set <source_file> <output_file>\n"
;

void* my_malloc(size_t size) {
  void* mem = malloc(size);
  if (!mem) {
    fprintf(stderr, "OUT OF MEMORY\n");
    exit(-1);
  }
  return mem;
}

void* my_realloc(void* ptr, size_t size) {
  void* mem = realloc(ptr, size);
  if (!mem) {
    fprintf(stderr, "OUT OF MEMORY\n");
    exit(-1);
  }
  return mem;
}

FILE* fopenSrc(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Unable to open source file: %s\n", path);
    fprintf(stderr, USAGE);
    exit(1);
  }
  return f;
}

FILE* fopenDst(const char* path) {
  FILE* f = fopen(path, "wt");
  if (!f) {
    fprintf(stderr, "Unable to open destination file: %s\n", path);
    fprintf(stderr, USAGE);
    exit(1);
  }
  return f;
}

// Reads tab-delimited fields from file, and writes them to fixed-width fields
// in buffer where fieldOffset = fieldIndex * fieldWidth. The size of
// fieldWidth should account for null terminator.
// Returns fieldCount on success; -1 on EOF; on error, the problem fieldIndex.
int readTableLine(FILE* src, int fieldCount, int fieldWidth, char* lineBuffer) {
  if (fieldWidth == 0)
    return 0; // can't have 0 fieldWidth
  int ch = 0;
  for (int fieldIndex=0; fieldIndex < fieldCount; fieldIndex++) {
    char* field = lineBuffer + fieldIndex * fieldWidth;
    int charIndex=0;
    for (; charIndex < fieldWidth; charIndex++) {
      ch = getc(src);
      if (ch == EOF) {
        if (fieldIndex == 0 && charIndex == 0)
          if (feof(src))
            return -1; // EOF at expected place
          else
            return fieldIndex; // read error
        else
          return fieldIndex; // read error or unexpected EOF
      }
      if (ch == '\t' || ch == '\r' || ch == '\n')
        break;
      field[charIndex] = ch;
    }
    if (ch != '\t' && fieldIndex != fieldCount - 1)
      return fieldIndex; // premature end of line
    if (charIndex == fieldWidth)
      return fieldIndex; // field data too long
    field[charIndex] = '\0';
  }
  return fieldCount; // success
}

void generateInstructionSet(const char* srcPath, const char* dstPath) {
#define fieldCount 3
#define fieldWidth 10
  FILE* src = fopenSrc(srcPath);
  FILE* dst = fopenDst(dstPath);
  char line[fieldCount * fieldWidth];
  const char* f1 = line + 0 * fieldWidth;
  const char* f2 = line + 1 * fieldWidth;
  const char* f3 = line + 2 * fieldWidth;
  char opcodeHex[17];
  int opcode = 0;
  for (;;) {
    int result = readTableLine(src, fieldCount, fieldWidth, (char*)line);
    if (result == -1) {
      break; // EOF
    }
    for (;;) {
      sprintf(opcodeHex, "%02X", opcode);
      opcode++;
      if (!strcmp(opcodeHex, f1)) {
        fprintf(dst, "/* %s */ { %s, AM_%s },\n", opcodeHex, f2, f3);
        break;
      } else {
        fprintf(dst, "/* %s */ { 0, 0 },\n", opcodeHex);
      }
    }
  }
  for (; opcode < 0x100; opcode++) {
    fprintf(dst, "/* %02X */ { 0, 0 },\n", opcode);
  }
#undef fieldCount
#undef fieldWidth
}

int main(int argc, char** argv) {
  if (argc != 4) {
    fprintf(stderr, "ERROR: Wrong number of arguments.\n");
    fprintf(stderr, USAGE);
    exit(1);
  }
  if (!strcmp(argv[1], "instruction_set")) {
    generateInstructionSet(argv[2], argv[3]);
  } else {
    fprintf(stderr, "ERROR: Invalid command.\n");
    fprintf(stderr, USAGE);
    exit(1);
  }
}

