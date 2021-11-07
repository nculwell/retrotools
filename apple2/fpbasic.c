// vim: et sw=2 sts=2

#include "shared.h"
#include "fpbasic.h"

static struct {
  Stream src;
} fpbasic;

void FpBasicPrintLineNumber(FILE* target, unsigned line_number) {
  fprintf(target, " %d ", line_number);
}

bool FpBasicListLine(FILE* dst) {
  FILE* target;
  if (LIST_TO_STDOUT) {
    target = stdout;
  } else {
    target = dst;
  }
  unsigned next_line_addr = ReadUint16(&fpbasic.src);
  if (next_line_addr == 0)
    return false;
  //printf("Next line addr: $%04X\n", next_line_addr);
  unsigned line_number = ReadUint16(&fpbasic.src);

  FpBasicPrintLineNumber(target, line_number);
  unsigned line_start_pos = fpbasic.src.pos; // save src.pos
  if (LIST_BINARY) {
    while (fpbasic.src.pos < fpbasic.src.len) {
      unsigned pos = fpbasic.src.pos - line_start_pos;
      bool newline = false;
      if (pos > 0 && pos % 16 == 0)
        newline = true;
      unsigned c = Read(&fpbasic.src);
      if (c == 0)
        break;
      if (newline)
        fprintf(target, "\n      ");
      fprintf(target, " %02X", c);
    }
    fprintf(target, "\n");
    fpbasic.src.pos = line_start_pos; // restore src.pos

    FpBasicPrintLineNumber(target, line_number);
  }
  while (fpbasic.src.pos < fpbasic.src.len) {
    unsigned c = Read(&fpbasic.src);
    if (c == 0)
      break;
    if (c >= APPLESOFT_TOKENS_BEGIN_INDEX && c <= APPLESOFT_TOKENS_END_INDEX) {
      unsigned token_index = c - APPLESOFT_TOKENS_BEGIN_INDEX;
      const char* token = APPLESOFT_TOKENS[token_index];
      fprintf(target, " %s ", token);
    } else {
      fprintf(target, "%c", c);
    }
  }
  fprintf(target, "\n");
  return true;
}

void FpBasicListProgram(const char* src_path, FILE* dst) {
  InitStream(&fpbasic.src);
  ReadFileWithLengthPrefix(src_path, &fpbasic.src, 0);
  while (fpbasic.src.pos < fpbasic.src.len) {
    //printf("POS = %d, LEN = %d\n", fpbasic.src.pos, fpbasic.src.len);
    if (!FpBasicListLine(dst))
      return;
  }
}

