// ACS Forth Decompiler
//
// Given the starting point of the Forth code in RAM, this will step through it
// and resolve all references. It will spit out anonymous labels, which you
// need to fill in.
//
// 1. Run the decompiler
// 2. It will generate a code file and a labels file.
// 3. It will process the code and labels files to generate a source file.
// 4. Edit the labels and generate source again to get better output.

// This code doesn't always check bounds, so it has some buffer overflows. It's
// not intended for parsing arbitrary data, it's intended for a few particular
// inputs.

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "align.h"

typedef uint8_t byte_t;
typedef uint16_t word_t;

static inline word_t toWord(byte_t lo, byte_t hi) {
  return lo | (hi << 8);
}

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 512
#endif

#ifndef RAM_SIZE
#define RAM_SIZE 0x10000
#endif

// Max length of a word or label, not counting null.
#define MAX_NAME_LEN 30

// Max size of reference buffers.
// This is probably way bigger than we need.
// We don't check bounds so it had better be big enough.
#define MAX_REFS 0x4000

// May need to increase this. Program will stop if it overflows.
#define LABEL_AREA_SIZE (RAM_SIZE)

const char* USAGE =
"Usage: %s <RamDumpPath> <StartAddressHex> <OutputPathBase>"
;

char LABEL_AREA[LABEL_AREA_SIZE];
int nextLabelOffset = 0;

enum {
  TF_SCANNED  = 0b000000000001,
  TF_DEF      = 0b000000000010,
  TF_COLON    = 0b000000000100,
  TF_VARIABLE = 0b000000001000,
  TF_CONST    = 0b000000010000,
  TF_USE      = 0b000000100000,
  TF_CALL     = 0b000001000000,
  TF_BRANCH   = 0b000010000000,
  TF_LOOP     = 0b000100000000,
  TF_STORE    = 0b001000000000,
  TF_AT       = 0b010000000000,
};

typedef uint16_t targetFlags_t;

typedef struct {
  byte_t ram[RAM_SIZE];
  byte_t refType[RAM_SIZE];
  targetFlags_t targetFlags[RAM_SIZE];
  const char* label[RAM_SIZE];
} Decompile;

// TODO: Load tokens from file?
enum ForthToken {

  R_NONE = 0, // byte has not been encountered by scanner yet
  R_UNKNOWN,  // pointer to unknown address, usually an unlabeled codefield
  R_CONT,     // this is the second byte of a two-byte object
  R_CONT_VAR, // this is the second byte of the value of a variable (might not be used)

  // Numeric values that follow Forth words
  R_VAL_LITERAL, R_VAL_BRANCH_OFFSET, R_VAL_STR_LENPFX, R_VAL_VARIABLE,

  R_COLON_REF, R_CONST_REF, R_VARIABLE_REF, R_USE_REF,
  R_COLON_DEF, R_CONST_DEF, R_VARIABLE_DEF, R_USE_DEF,

  R_LIT, R_BRANCH, R_ZBRANCH, R_PLOOP, R_PPLOOP, R_EXEC_FOLLOWING,

#include "forth_words_defs.inc"

  FORTH_TOKEN_COUNT // use to size array of names

};

char* TOKEN_NAMES[FORTH_TOKEN_COUNT];

// This function maps addresses to known Forth routines.
int resolveRef(Decompile* d, unsigned ref, int* following);

// Associate names with token IDs.
void initTokenNames();

// Exit program with error message.
void fail(const char* fmt, ...)
  __attribute__((noreturn, format(printf, 1, 2)))
;

void fail(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  putc('\n', stderr);
  exit(1);
}

// Exit program with error message.
void warn(const char* fmt, ...)
  __attribute__((format(printf, 1, 2)))
;

void warn(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  putc('\n', stderr);
}

// Write debugging output.
void trace(const char* fmt, ...);

void trace(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  putc('\n', stderr);
}

// LABELING

#define LABEL_ALIGN 4
const char* addLabel(const char* label) {
  size_t len = strlen(label);
  size_t lenWithNull = len + 1;
  if (nextLabelOffset + lenWithNull >= LABEL_AREA_SIZE)
    fail("Label space overflow, increase LABEL_AREA_SIZE.");
  char* newLabel = LABEL_AREA + nextLabelOffset;
  memcpy(newLabel, label, lenWithNull);
  nextLabelOffset += lenWithNull;
  nextLabelOffset = nextAligned(nextLabelOffset, LABEL_ALIGN);
  return newLabel;
}

// PREPARE IN/OUT FILES

FILE* openFileWrite(const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) fail("Unable to open file: %s", path);
  return f;
}

void readRamDumpFile(const char* ramDumpPath, void* data, size_t len) {
  FILE* ramFile = fopen(ramDumpPath, "rb");
  if (!ramFile) fail("Unable to open RAM dump file: %s", ramDumpPath);
  size_t nRead = fread(data, 1, len, ramFile);
  if (nRead != len) fail("Error reading RAM dump file: %s", ramDumpPath);
}

void buildOutputPaths(const char* outputPathBase, char* codePath, char* labelsPath) {
  int outputPathLen = strlen(outputPathBase);
  if (outputPathLen + sizeof(".labels") >= MAX_PATH_LEN)
    fail("Output path is too long.");
  sprintf(codePath, "%s%s", outputPathBase, ".code");
  sprintf(labelsPath, "%s%s", outputPathBase, ".labels");
}

unsigned parseHex(const char* text) {
  unsigned n = 0;
  int c;
  int i;
  for (i=0; i < 8 && text[i] != 0; i++) {
    n <<= 4;
    c = text[i];
    c -= '0';
    if (c < 0)
      goto invalid;
    if (c < 10) {
      n += c; // digit 0-9
      continue;
    }
    c -= ('A' - '0');
    if (c < 0)
      goto invalid;
    if (c > ('a' - 'A'))
      c &= ('a' - 'A') - 1; // handle lowercase
    if (c <= ('F' - 'A')) {
      n += c + 10; // digit A-F
      continue;
    }
    goto invalid;
  }
  if (text[i] != 0)
    fail("Hex number too long: '%s'", text);
  return n;
invalid:
  fail("Invalid hex digit: $%04X", text[i]);
}

// Read labels if file is readable.
void readLabels(Decompile* d, const char* labelsPath) {
  FILE* f = fopen(labelsPath, "rb");
  if (!f)
    return;
  char lineBuf[1024];
  int lineOff = 0;
  int c;
  while ((c = getc(f)) != EOF) {
    if (c == '\n') {
      if (lineBuf[4] != ' ')
        fail("Invalid labels file, expected space in 5th column.");
      // split line into address and label text
      lineBuf[4] = 0;
      lineBuf[lineOff] = 0;
      unsigned addr = parseHex(lineBuf);
      const char* label = addLabel(lineBuf+5);
      // save label
      d->label[addr] = label;
      // reset for next line
      lineOff = 0;
    } else if (c == '\r') {
      // ignore
    } else {
      lineBuf[lineOff++] = c;
    }
  }
  if (ferror(f))
    fail("Error reading labels file: %s", labelsPath);
  fclose(f);
}

void openFiles(
    Decompile* d,
    const char* ramDumpPath, const char* outputPathBase,
    FILE** codeFile, FILE** labelsFile)
{
  char codePath[MAX_PATH_LEN];
  char labelsPath[MAX_PATH_LEN];
  readRamDumpFile(ramDumpPath, d->ram, RAM_SIZE);
  buildOutputPaths(outputPathBase, codePath, labelsPath);
  readLabels(d, labelsPath);
  *codeFile = openFileWrite(codePath);
  *labelsFile = openFileWrite(labelsPath);
}

const char* renderC64ScreenText(const byte_t* src, int len) {
  static char buf[0x1000];
  if (len >= 0x1000)
    return "<STRING TOO LONG TO DISPLAY>";
  for (int i=0; i < len; i++) {
    byte_t c = src[i];
    buf[i] = (char)(c < 0x20 ? c + 0x40 : c);
  }
  buf[len] = 0;
  return buf;
}

// DECOMPILER LOGIC

unsigned readWord(Decompile* d, unsigned ptr) {
  if (ptr > RAM_SIZE - 2)
    fail("Attempt to reference out-of-bounds address: %04X", ptr);
  return toWord(d->ram[ptr], d->ram[ptr+1]);
}

unsigned processRef(
    Decompile* d,
    unsigned ptrBegin,
    unsigned refAtPtr,
    unsigned refType,
    unsigned following,
    unsigned* newRefs,
    int* newRefsCount)
{
  unsigned ptr = ptrBegin;
  d->refType[ptr+1] = R_CONT; // mark second byte of cell
  ptr += 2; // advance past current cell

  switch (refType) {
    case R_BRANCH:
    case R_ZBRANCH:
      d->targetFlags[refAtPtr] |= TF_BRANCH;
      break;
    case R_PLOOP:   // (LOOP) is compiled by LOOP
    case R_PPLOOP:  // (+LOOP) is compiled by +LOOP
      d->targetFlags[refAtPtr] |= TF_LOOP;
      break;
    case R_COLON_DEF:
      d->targetFlags[refAtPtr] |= TF_DEF | TF_COLON;
      break;
    case R_VARIABLE_DEF:
      d->targetFlags[refAtPtr] |= TF_DEF | TF_VARIABLE;
      break;
    case R_CONST_DEF:
      d->targetFlags[refAtPtr] |= TF_DEF | TF_CONST;
      break;
    case R_USE_DEF:
      d->targetFlags[refAtPtr] |= TF_DEF | TF_USE;
      break;
  }

  if (following > 0) {
    if (following == R_VAL_STR_LENPFX) {
      // Character string with 1-byte length prefix.
      int len = d->ram[ptr];
      const char* str = renderC64ScreenText(&d->ram[ptr+1], len);
      trace("      STR: [%d] \"%s\"", len, str);
      d->refType[ptr++] = following;
      unsigned strLenPtr = ptr;
      for (int i=0; i < len; i++) {
        d->refType[ptr++] = following;
      }
      assert(ptr == strLenPtr + len);
    } else {
      // Two-byte following number.
      d->refType[ptr] = following;
      if (following == R_VAL_BRANCH_OFFSET) {
        d->refType[ptr+1] = R_CONT; // mark second byte of following number
        // Must interpret the branch offset as a SIGNED 16-bit int to get the
        // right value (which can be negative).
        int offset = (int16_t)readWord(d, ptr);
        int addr = (int)ptr + offset;
        trace("      BRANCH: offset %+d => %04X", offset, addr);
        if (addr < 0 || addr >= RAM_SIZE)
          fail("Branch target out of range: %+d", addr);
        // Add this address to the refs list.
        newRefs[(*newRefsCount)++] = addr;
      } else {
        assert(following == R_VAL_LITERAL || following == R_VAL_VARIABLE);
        if (following == R_VAL_VARIABLE) {
          // Variable values are sometimes just one byte long.
          if (d->refType[ptr+1] == R_NONE)
            d->refType[ptr+1] = R_CONT_VAR;
        } else {
          unsigned byte2type = d->refType[ptr+1];
          if (byte2type != R_NONE)
            fail("Value following %s collides with assigned byte of type %s [%d].",
                TOKEN_NAMES[refType], TOKEN_NAMES[byte2type], byte2type);
          d->refType[ptr+1] = R_CONT;
        }
        // Don't keep processing after these words because the IP should
        // never actually point to them.
        if (refType == R_VARIABLE_DEF || refType == R_CONST_DEF
            || refType == R_USE_DEF)
        {
          return 0;
        }
      }
      ptr += 2; // advance past following cell
    }
  }

  return ptr;

}

void scanDefinition(Decompile* d, unsigned ref, unsigned* newRefs, int* newRefsCount) {
  trace("  Scan: %04X", ref);
  unsigned ptr = ref;
  if (d->refType[ptr] != R_NONE) {
    // TODO: Allow if type is R_CONT following certain types of objects.
    // Probably just VARIABLE values.
    trace("    Already scanned, skipping.");
    return;
  }
  d->targetFlags[ptr] |= TF_SCANNED;
  int refType;
  int prevRefType = 0;
  int repeatCount = 0;
  do {
    unsigned refAtPtr = readWord(d, ptr);
    if (refAtPtr >= RAM_SIZE) fail("Out of range: %X", refAtPtr);
    int following = 0;

    // This is where we identify what the cell at the pointer is referencing.
    refType = resolveRef(d, refAtPtr, &following);
    d->refType[ptr] = refType;

    // If this cell references something else that we need to scan, add it to
    // the "new references" list.
    bool added = false;
    if (refType == R_COLON_REF || refType == R_VARIABLE_REF
        || refType == R_CONST_REF || refType == R_USE_REF)
    {
      unsigned valAtPtr = readWord(d, refAtPtr);
      unsigned indirectRefType = d->refType[valAtPtr];
      if (indirectRefType == R_NONE) {
        newRefs[(*newRefsCount)++] = refAtPtr;
        added = true;
      }
    }

    // Report the basic info for this ref in the trace. We don't have all the
    // info here, but we want to print what we know now in case we stop or
    // things go wrong afterward. The trace doesn't have to have all the info
    // because it's not the final product; it's just used to spot things going
    // wrong.
    trace("    Line %04X: %04X -- %02X %s%s",
        ptr, refAtPtr, refType, TOKEN_NAMES[refType],
        added ? " (ADDED)" : "");

    if (refType == R_UNKNOWN) {
      // Don't continue after a reference to unknown native code. Encountering
      // this could mean our interpretation has already gone off the rails
      // (e.g. something manipulated the stack and we missed it) and we're now
      // interpreting garbage. If this is a reference to real native code then
      // we need to add it to the code to fix the error before proceeding.
      trace("    <aborting scan of this definition>");
      break;
    }

    d->refType[ptr+1] = R_CONT; // mark second byte of cell

    // Do all the work that's specific to certain reference types.
    ptr = processRef(d, ptr, refAtPtr, refType, following, newRefs, newRefsCount);

    // 0 means we're done processing this definition. (0 is not otherwise a
    // valid return value here. Even if we could locate code at position 0,
    // which we can't, the pointer would've advanced past it by now.)
    if (ptr == 0)
      break;

    // TODO: Get rid of this, I don't think I need it now that the loop halts
    // upon seeing a single unknown ref.
    if (prevRefType == refType) {
      repeatCount++;
    } else {
      prevRefType = refType;
      repeatCount = 0;
    }
    if (repeatCount == 20)
      fail("Too many repeats, something went wrong.");

  } while (refType != R_SEMICOLON);
}

void decompileLoop(Decompile* d, unsigned startAddr) {
  int iteration = 0;
  unsigned refsBuf1[0x400];
  unsigned refsBuf2[0x400];
  unsigned* newRefs = refsBuf1;
  unsigned* oldRefs = refsBuf2;
  newRefs[0] = startAddr;
  int newRefsCount = 1;
  while (newRefsCount > 0) {
    iteration++;
    // Swap buffers
    int oldRefsCount = newRefsCount;
    trace("Round %d: %d", iteration, oldRefsCount);
    newRefsCount = 0;
    oldRefs = (oldRefs == refsBuf1) ? refsBuf2 : refsBuf1;
    newRefs = (newRefs == refsBuf1) ? refsBuf2 : refsBuf1;
    // Process oldRefs and build newRefs
    for (int i=0; i < oldRefsCount; i++) {
      scanDefinition(d, oldRefs[i], newRefs, &newRefsCount);
    }
  }
}

void fillNameField(char* field, const char* value) {
  size_t i=0;
  for (; i < strlen(value); i++) {
    field[i] = value[i];
  }
  for (; i < MAX_NAME_LEN; i++) {
    field[i] = ' ';
  }
  field[MAX_NAME_LEN] = 0;
}

const char* getLabel(Decompile* d, word_t addr) {
  static char buf[128];
  char* p = buf;
  assert(addr);
  const char* lbl = d->label[addr];
  if (lbl) return lbl;
  int rt = d->refType[addr];
  const char* pfx;
  switch (rt) {
    case R_COLON_REF:
      addr = readWord(d, addr); 
      lbl = d->label[addr];
      if (lbl) return lbl;
      __attribute__((fallthrough));
    case R_COLON_DEF:
      pfx = "COLON"; break;
    case R_VARIABLE_REF:
      addr = readWord(d, addr); 
      lbl = d->label[addr];
      if (lbl) return lbl;
      __attribute__((fallthrough));
    case R_VARIABLE_DEF:
      pfx = "VAR"; break;
    case R_CONST_REF:
      addr = readWord(d, addr); 
      lbl = d->label[addr];
      if (lbl) return lbl;
      __attribute__((fallthrough));
    case R_CONST_DEF:
      pfx = "CONST"; break;
    case R_USE_REF:
      addr = readWord(d, addr); 
      lbl = d->label[addr];
      if (lbl) return lbl;
      __attribute__((fallthrough));
    case R_USE_DEF:
      pfx = "USE"; break;
    default:
      return NULL;
  }
  p += sprintf(p, "%s_%04X", pfx, addr);
  if (rt == R_CONST_REF || rt == R_CONST_DEF || rt == R_USE_REF || rt == R_USE_DEF)
    p += sprintf(p, "_%04X", readWord(d, addr+2));
  return buf;
}

void writeOutput(Decompile* d, FILE* codeFile, FILE* labelsFile) {
  char labelPadded[MAX_NAME_LEN+1];
  char wordPadded[MAX_NAME_LEN+1];
  // TODO: Print labels.
  unsigned prev = R_NONE; 
  for (unsigned ptr = 0; ptr < RAM_SIZE-2; ptr++) {
    unsigned t = d->refType[ptr];
    fillNameField(labelPadded, "");
    if (t == R_CONT) {
      // skip
    } else if (t == R_NONE) {
      if (prev == R_NONE) {
        // do nothing, we're in a non-Forth block
      } else {
        fprintf(codeFile, "\n");
      }
    } else {
      if (prev == R_NONE) {
        // Starting a new Forth block. Don't really need a message here but
        // we'll add one to show that we can recognize it.
        //fprintf(codeFile, "\\ NEW BLOCK\n");
      }
      const char* label = getLabel(d, ptr);
      if (t == R_VAL_STR_LENPFX) {
        int len = d->ram[ptr];
        const char* str = renderC64ScreenText(&d->ram[ptr+1], len);
        fprintf(codeFile, "%04X:  %02X... %s  \"%s\"",
            ptr, len, labelPadded, str);
        ptr += len;
      } else if (t == R_VAL_LITERAL) {
        word_t litVal = readWord(d, ptr);
        fillNameField(labelPadded, "");
        int len = sprintf(labelPadded, "$%04X", litVal);
        labelPadded[len] = ' ';
        fprintf(codeFile, "%04X:  %04X  %s  $%04X",
            ptr, litVal, labelPadded, litVal);
      } else if (t == R_VAL_VARIABLE) {
        word_t varVal = d->ram[ptr];
        if (d->refType[ptr+1] == R_CONT_VAR) {
          word_t firstPtr = ptr;
          ptr++;
          varVal |= (d->ram[ptr] << 8);
          fprintf(codeFile, "%04X:  %04X  %s  %04X",
              firstPtr, varVal, labelPadded, varVal);
        } else {
          fprintf(codeFile, "%04X:  %02X    %s  %02X",
              ptr, varVal, labelPadded, varVal);
        }
      } else if (t == R_VAL_BRANCH_OFFSET) {
        word_t raw = readWord(d, ptr);
        int offset = (int16_t)raw;
        int branchAddr = (int)ptr + offset;
        fprintf(codeFile, "%04X:  %04X  %s  %+d => %04X",
            ptr, raw, labelPadded, offset, branchAddr);
        d->targetFlags[branchAddr] |= TF_BRANCH;
      } else {
        unsigned rt = d->refType[ptr];
        const char* name = TOKEN_NAMES[rt];
        bool isDef = (t == R_COLON_DEF || t == R_VARIABLE_DEF || t == R_CONST_DEF || t == R_USE_DEF);
        if (isDef)
          fprintf(codeFile, "\n");
        word_t addr = readWord(d, ptr);
        if (label) {
          fillNameField(labelPadded, label);
        } else {
          const char* effectiveLabel;
          if (rt == R_BRANCH || rt == R_ZBRANCH)
            effectiveLabel = "";
          else if (rt == R_SEMICOLON)
            effectiveLabel = ";";
          else
            effectiveLabel = name;
          fillNameField(labelPadded, effectiveLabel);
        }
        fillNameField(wordPadded, name);
        fprintf(codeFile, "%04X:  %04X  %s  %s",
            ptr, addr, labelPadded, wordPadded);
        if (isDef && label && strlen(label) > 0) {
          fprintf(labelsFile, "%04X %s\n", ptr, label);
        }
      }
      fprintf(codeFile, "\n");
    }
    prev = t;
  }
}

// Fill in labels.
void label(Decompile* d) {

  const char* labelWHILE = addLabel("WHILE");
  const char* labelBEGIN = addLabel("BEGIN");
  const char* labelUNTIL = addLabel("UNTIL");
  const char* labelIF = addLabel("IF");
  const char* labelELSE = addLabel("ELSE");
  const char* labelENDIF = addLabel("ENDIF"); // = THEN
  const char* labelREPEAT = addLabel("REPEAT");

#define DO_STACK_SIZE 5
  assert(d);
  unsigned doStack[DO_STACK_SIZE];
  int doStackCount = 0;
  for (unsigned ptr = 0; ptr < RAM_SIZE; ptr++) {
    switch (d->refType[ptr]) {

      // Loop patterns:
      // DO ... can use I here ... LOOP
      // DO ... can use I here ... n +LOOP

      case R_PDO:
        if (doStackCount == DO_STACK_SIZE)
          fail("DO stack overflow.");
        doStack[doStackCount] = ptr;
        ++doStackCount;
        break;

      case R_PLOOP:
      case R_PPLOOP:
        {
          int offset = (int16_t)readWord(d, ptr+2);
          unsigned toAddr = ptr + 2 + offset;
          unsigned expectedDoAddr = toAddr - 2;
          int expectedDoRefType = d->refType[expectedDoAddr];
          if (expectedDoRefType != R_PDO)
            warn("At %04X: (LOOP) dest is not (DO), it's %04X %s [%d]",
                ptr, toAddr, TOKEN_NAMES[expectedDoRefType], expectedDoRefType);
          if (doStackCount == 0)
            fail("At %04X: DO stack underflow.", ptr);
          --doStackCount;
          unsigned addrOnStack = doStack[doStackCount];
          if (addrOnStack != expectedDoAddr)
            warn("At %04X: (LOOP) dest doesn't match address on DO stack (%04X).", ptr, addrOnStack);
        }
        break;

      // Branching patterns:
      // BEGIN ... cond WHILE ... REPEAT
      //   => [begin] ... cond 0BRANCH after ... BRANCH begin [after]
      //   i.e. 0branch forward; dest is preceded by branch backward
      // BEGIN ... UNTIL
      //   => [begin] ... cond 0BRANCH
      //   i.e. 0branch backward
      // IF [true_part] ... ENDIF [after]
      //   => 0BRANCH after ... [after]
      //   i.e. 0branch forward
      // IF [true_part] ... ELSE [false_part] ... ENDIF
      //   => 0BRANCH false_part ... BRANCH after [false_part] ... [after]
      //   i.e. 0branch forward; dest is preceded by branch forward

      case R_ZBRANCH:
        {
          int offset = (int16_t)readWord(d, ptr+2);
          unsigned toAddr = ptr + 2 + offset;
          if (offset < 0) {
            d->label[toAddr] = labelBEGIN;
            d->label[ptr] = labelUNTIL;
          } else {
            unsigned beforeToAddr = toAddr - 4;
            // TODO: Check range
            int befToAddrRefType = d->refType[beforeToAddr];
            if (befToAddrRefType == R_BRANCH) {
              int befToAddrOffset = (int16_t)readWord(d, beforeToAddr+2);
              unsigned befToAddrToAddr = beforeToAddr + 2 + befToAddrOffset;
              if (befToAddrOffset < 0) {
                d->label[ptr] = labelBEGIN;
                d->label[toAddr] = labelWHILE;
                d->label[befToAddrToAddr] = labelREPEAT;
              } else {
                d->label[ptr] = labelIF;
                d->label[toAddr] = labelELSE;
                d->label[befToAddrToAddr] = labelENDIF;
              }
            } else {
              d->label[ptr] = labelIF;
              d->label[toAddr] = labelENDIF;
            }
          }
          ptr += 2;
        }
        break;

    }
  }
}

// Read RAM and generate code and labels.
void decompile(const char* ramDumpPath, unsigned startAddr, const char* outputPathBase) {
  Decompile d;
  FILE* codeFile;
  FILE* labelsFile;
  memset(&d, 0, sizeof(d));
  openFiles(&d, ramDumpPath, outputPathBase, &codeFile, &labelsFile);
  decompileLoop(&d, startAddr);
  label(&d);
  writeOutput(&d, codeFile, labelsFile);
}

long parseInt(const char* s) {
  char* end;
  errno = 0;
  long n = strtol(s, &end, 0);
  if (n == LONG_MIN) fail("Underflow: %s", s);
  if (n == LONG_MAX) fail("Overflow: %s", s);
  if (errno != 0) fail("Invalid number '%s': %s", s, strerror(errno));
  return n;
}

// Args: ram dump path; start address; output path base
int main(int argc, char** argv) {
  if (argc != 4)
    fail(USAGE, argv[0]);
  unsigned startAddr = (unsigned)parseInt(argv[2]);
  initTokenNames();
  decompile(argv[1], startAddr, argv[3]);
}

// These are addresses of code. Forth doesn't really interpret them
// differently. What's different is that instead of having code point to a
// single code field for each of these words, there are direct code references
// all over the place. The code is then sensitive to the IP value, using it to
// retrieve the following data.
int resolveIndirect(Decompile* d, unsigned ref, int* following) {
  *following = 0;
  unsigned valAtRef = readWord(d, ref);
  // TODO: Also mark type of CONST, VARIABLE and USE lines.
  switch (valAtRef) {
    case 0x08E2: return R_COLON_REF;
    case 0x08F9: return R_CONST_REF;
    case 0x0906: return R_VARIABLE_REF;
    case 0x0914: return R_USE_REF;
  }
  return R_UNKNOWN;
}

int resolveRef(Decompile* d, unsigned ref, int* following) {
  *following = 0;
  switch (ref) {

    // words that consume the following cell
    case 0x08AA: *following = R_VAL_LITERAL; return R_LIT;
    case 0x0966: *following = R_VAL_BRANCH_OFFSET; return R_BRANCH;
    case 0x097B: *following = R_VAL_BRANCH_OFFSET; return R_ZBRANCH;
    case 0x0993: *following = R_VAL_BRANCH_OFFSET; return R_PLOOP;
    case 0x09B9: *following = R_VAL_BRANCH_OFFSET; return R_PPLOOP;
    case 0x1870: // another codefield for same code, I don't see this one used
    case 0x4DF9: // another codefield for same code, this one is called
    case 0x7054: // another codefield for same code, this one is called
    case 0x3D82: // *following = R_VAL_LITERAL;
                 // Technically EXEC_FOLLOWING should consume the following
                 // cell, but since that cell will be a routine to execute,
                 // it's going to work better if we just parse it as regular
                 // code.
                 return R_EXEC_FOLLOWING;

    // words with special following behavior (colon refs)
    case 0x45EE: *following = R_VAL_STR_LENPFX; return R_COLON_REF;

    // words that act normally
    case 0x08E2: return R_COLON_DEF; // reference to any colon def
    case 0x08F9: *following = R_VAL_LITERAL; return R_CONST_DEF;
    case 0x0906: *following = R_VAL_VARIABLE; return R_VARIABLE_DEF;
    case 0x0914: *following = R_VAL_LITERAL; return R_USE_DEF;

#include "forth_words_addrs.inc"

    // If the word isn't recognized, dereference it and analyze it by where it
    // points to.
    default: return resolveIndirect(d, ref, following);
  }
}

void initTokenNames() {
  for (int i=0; i < FORTH_TOKEN_COUNT; i++) {
    TOKEN_NAMES[i] = "<NO_NAME>"; // make sure undefined names are printable
  }

  TOKEN_NAMES[R_UNKNOWN] = "<UNKNOWN>";

  TOKEN_NAMES[R_COLON_REF] = "COLON REF";
  TOKEN_NAMES[R_VARIABLE_REF] = "VARIABLE REF";
  TOKEN_NAMES[R_CONST_REF] = "CONST REF";
  TOKEN_NAMES[R_USE_REF] = "USE REF";
  TOKEN_NAMES[R_ZBRANCH] = "0BRANCH";
  TOKEN_NAMES[R_BRANCH] = "BRANCH";
  TOKEN_NAMES[R_PLOOP] = "(LOOP)";
  TOKEN_NAMES[R_PPLOOP] = "(+LOOP)";
  TOKEN_NAMES[R_LIT] = "LITERAL";
  TOKEN_NAMES[R_EXEC_FOLLOWING] = "EXEC_FOLLOWING";

  TOKEN_NAMES[R_COLON_DEF] = "COLON";
  TOKEN_NAMES[R_VARIABLE_DEF] = "VARIABLE";
  TOKEN_NAMES[R_CONST_DEF] = "CONST";
  TOKEN_NAMES[R_USE_DEF] = "USE";

  TOKEN_NAMES[R_SEMICOLON] = "SEMICOLON";

#include "forth_words_names.inc"

  // Override names from file.
  //TOKEN_NAMES[R_PDO] = "(DO)";
  //TOKEN_NAMES[R_AT] = "@";

  // Check max name length.
  for (int i=0; i < FORTH_TOKEN_COUNT; i++) {
    if (strlen(TOKEN_NAMES[i]) >= MAX_NAME_LEN)
      fail("Name is too long: %s", TOKEN_NAMES[i]);
  }
}

