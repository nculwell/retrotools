// vim: et sw=2 sts=2

#include "shared.h"

static struct {
  Stream src;
} intbasic;

void ListLine(int line_length, FILE* dst) {
  FILE* target;
  if (LIST_TO_STDOUT) {
    target = stdout;
  } else {
    target = dst;
  }
  unsigned line_number = ReadUint16(&intbasic.src);
  fprintf(target, "%5d ", line_number);
  unsigned line_start_pos = intbasic.src.pos; // save intbasic.src.pos
  unsigned line_end_pos = line_start_pos + line_length - 3;
  if (LIST_BINARY) {
    while (intbasic.src.pos < line_end_pos) {
      unsigned pos = intbasic.src.pos - line_start_pos;
      if (pos > 0 && pos % 16 == 0)
        fprintf(target, "\n      ");
      unsigned c = Read(&intbasic.src);
      fprintf(target, " %02X", c);
    }
    fprintf(target, "\n");
    intbasic.src.pos = line_start_pos; // restore intbasic.src.pos
    fprintf(target, "%5d ", line_number);
  }
  while (intbasic.src.pos < line_end_pos) {
    unsigned c = Read(&intbasic.src);
    switch (c) {

      case 0x01: // end of line
        if (intbasic.src.pos != line_end_pos)
          Die("Premature end of line (i=%d,end=%d).", intbasic.src.pos, line_end_pos);
        break;
      case 0x03: fprintf(target, ":"); break;
      case 0x09: fprintf(target, " DEL "); break;
      case 0x0a: fprintf(target, ","); break; // in DEL args
      case 0x12: fprintf(target, "+"); break;
      case 0x13: fprintf(target, "-"); break;
      case 0x14: fprintf(target, "*"); break;
      case 0x15: fprintf(target, "/"); break;
      case 0x16: fprintf(target, "="); break;
      case 0x17: fprintf(target, "#"); break; // not equals
      case 0x18: fprintf(target, ">="); break;
      case 0x19: fprintf(target, ">"); break;
      case 0x1a: fprintf(target, "<="); break;
      case 0x1b: fprintf(target, "<>"); break; // not equals (numbers only)
      case 0x1c: fprintf(target, "<"); break;
      case 0x1d: fprintf(target, " OR "); break;
      case 0x1e: fprintf(target, " OR "); break;
      case 0x1f: fprintf(target, " MOD "); break;
      case 0x20: fprintf(target, " ^ "); break;
      case 0x22: fprintf(target, "("); break; // open paren in DIM for string array
      case 0x23: fprintf(target, ","); break; // comma in substring args
      case 0x24: fprintf(target, " THEN "); break; // implicit goto after if-then
      case 0x25: fprintf(target, " THEN "); break;
      case 0x26: fprintf(target, ","); break; // INPUT arg separator
      case 0x27: fprintf(target, ","); break;
      case 0x28: // open quote
        {
          fprintf(target, "\"");
          while ((c = Read(&intbasic.src)) != 0x29) {
            fprintf(target, TranslateChar(c));
          }
          fprintf(target, "\""); // 0x29: close quote
          break;
        }
      case 0x29: Die("Unexpected $29 close quote with no $28 open quote.");
      case 0x2a: fprintf(target, "("); break; // open paren (substring)
      case 0x2d: fprintf(target, "("); break; // open paren (array subscript)
      case 0x2e: fprintf(target, " PEEK"); break;
      case 0x2f: fprintf(target, " RND "); break;
      case 0x30: fprintf(target, " SGN "); break;
      case 0x34: fprintf(target, "("); break; // open paren in DIM for int array
      case 0x35: fprintf(target, " +"); break; // unary plus
      case 0x36: fprintf(target, " -"); break; // unary minus
      case 0x37: fprintf(target, " NOT "); break;
      case 0x38: fprintf(target, "("); break;
      case 0x39: fprintf(target, "="); break; // str array elt equal (in expr?)
      case 0x3a: fprintf(target, " AND "); break;
      case 0x3b: fprintf(target, " LEN("); break;
      case 0x3d: fprintf(target, " SCRN("); break;
      case 0x3e: fprintf(target, ","); break; // in SCRN args
      case 0x3f: fprintf(target, "("); break; // open paren (peek arg, sgn arg)
      case 0x40: fprintf(target, "$"); break; // string var
      case 0x45: fprintf(target, ";"); break; // print args separator (what follows is string)
      case 0x46: fprintf(target, ";"); break; // print args separator (what follows is int)
      case 0x47: fprintf(target, ";"); break;
      case 0x48: fprintf(target, ","); break; // print args separator (seen before int var)
      case 0x4b: fprintf(target, " TEXT "); break;
      case 0x4c: fprintf(target, " GR "); break;
      case 0x4d: fprintf(target, " CALL "); break;
      case 0x4e: fprintf(target, " DIM "); break; // string array
      case 0x4f: fprintf(target, " DIM "); break; // int array
      case 0x50: fprintf(target, " TAB "); break;
      case 0x51: fprintf(target, " END "); break;
      case 0x52: fprintf(target, " INPUT "); break; // input with int arg
      case 0x53: fprintf(target, " INPUT "); break; // input with prompt (what kind of arg?)
      case 0x54: fprintf(target, " INPUT "); break; // input with string arg
      case 0x55: fprintf(target, " FOR "); break;
      case 0x56: fprintf(target, " = "); break; // for stmt
      case 0x57: fprintf(target, " TO "); break; // for stmt
      case 0x58: fprintf(target, " STEP "); break; // for stmt
      case 0x59: fprintf(target, " NEXT "); break;
      case 0x5b: fprintf(target, " RETURN "); break;
      case 0x5c: fprintf(target, " GOSUB "); break;
      case 0x5d:
        {
          fprintf(target, " REM ");
          while (intbasic.src.pos < line_end_pos - 1) {
            fprintf(target, TranslateChar(Read(&intbasic.src)));
          }
          break;
        }
      case 0x5f: fprintf(target, " GOTO "); break;
      case 0x60: fprintf(target, " IF "); break;
      case 0x61: fprintf(target, " PRINT "); break; // 1 arg
      case 0x62: fprintf(target, " PRINT "); break; // multiple args (; separator)
      case 0x63: fprintf(target, " PRINT "); break; // no args
      case 0x64: fprintf(target, " POKE "); break;
      case 0x65: fprintf(target, ","); break; // in POKE args
      case 0x66: fprintf(target, " COLOR= "); break;
      case 0x67: fprintf(target, " PLOT "); break;
      case 0x68: fprintf(target, ","); break; // in PLOT args
      case 0x69: fprintf(target, " HLIN "); break;
      case 0x6a: fprintf(target, ","); break; // in HLIN
      case 0x6b: fprintf(target, " AT "); break; // in HLIN
      case 0x6c: fprintf(target, " VLIN "); break;
      case 0x6d: fprintf(target, ","); break; // in VLIN
      case 0x6e: fprintf(target, " AT "); break; // in VLIN
      case 0x6f: fprintf(target, " VTAB "); break;
      case 0x70: fprintf(target, "="); break; // string assignment
      case 0x71: fprintf(target, "="); break; // number assignment
      case 0x72: fprintf(target, ")"); break; // close paren (substring, peek arg)
      case 0x77: fprintf(target, " POP "); break;
      case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4:
      case 0xb5: case 0xb6: case 0xb7: case 0xb8: case 0xb9:
        {
          // number
          unsigned first_digit = c - 0xb0;
          unsigned number_value = ReadUint16(&intbasic.src);
          char number_display[6];
          sprintf(number_display, "%d", number_value);
          fprintf(target, number_display);
          if (!((unsigned)number_display[0] == first_digit + '0'))
            Die("Number's first digit '%c' doesn't match expected value '%c'"
                " (line %d, byte $%02X)",
                number_display[0], TranslateChar(c), line_number,
                intbasic.src.pos-line_start_pos-3);
        }
        break;
      default:
        // letter (variable name)
        if (c >= ('A'|0x80) && c <= ('Z'|0x80)) {
          while (
              (c >= ('A'|0x80) && c <= ('Z'|0x80))
              || (c >= ('0'|0x80) && c <= ('9'|0x80))
              )
          {
            fprintf(target, "%c", c & 0x7F);
            c = Read(&intbasic.src);
          }
          intbasic.src.pos--; // put back the last char
        } else {
          Die("Unexpected Integer BASIC token $%02X (line %d, offset $%02X)",
              c, line_number, intbasic.src.pos-line_start_pos-1);
        }
        break;

    } // end switch
  }
  fprintf(target, "\n");
}

void ListProgram(const char* src_path, FILE* dst) {
  InitStream(&intbasic.src);
  ReadFileWithLengthPrefix(src_path, &intbasic.src, 0);
  intbasic.src.pos = 0;
  unsigned len = ReadUint16(&intbasic.src); // advance past length
  printf("%d / %d\n", len, intbasic.src.len);
  while (intbasic.src.pos < intbasic.src.len) {
    unsigned line_length = Read(&intbasic.src);
    ListLine(line_length, dst);
    // intbasic.src.pos += line_length;
  }
}

