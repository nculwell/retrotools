
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "em.h"

#define FILE_BLOCK_SIZE 256

static char hexDigits[] = "0123456789ABCDEF";

word_t parseAddr(const char* s) {
  word_t addr = 0;
  size_t len = strlen(s);
  if (len == 0 || len > 4) {
    fprintf(stderr, "Invalid address.\n");
    exit(1);
  }
  for (size_t i=0; i < len; i++) {
    const char* p = strchr(hexDigits, toupper(s[i]));
    if (p == NULL) {
      fprintf(stderr, "Invalid address.\n");
      exit(1);
    }
    int c = p - hexDigits;
    addr |= c << (4 * (len - i - 1));
  }
  return addr;
}

//#pragma GCC diagnostic ignored "-Wunused-variable"

buf_t* readFileOrFail(const char* path, const char* fileDescr) {
  buf_t* file = readFile(path);
  if (!file) {
    fprintf(stderr, "Unable to load %s file: %s\n", fileDescr, path);
    exit(2);
  }
  return file;
}

int main(int argc, char** argv) {
  emu_t* m = createEmulator(stdout);
  if (argc > 1 && !strcmp("state", argv[1])) {
    // process a state file
    if (argc != 5) {
      fprintf(stderr, "State file usage:\n  c64emulator state REG_PATH RAM_PATH DISK_PATH\n");
      return 2;
    }
    const char* regPath = argv[2];
    const char* ramPath = argv[3];
    const char* diskPath = argv[4];
    buf_t* regFile = readFileOrFail(regPath, "register");
    buf_t* ramFile = readFileOrFail(ramPath, "RAM");
    buf_t* diskFile = readFileOrFail(diskPath, "disk");
    loadRegisters(m, regFile);
    loadRAM(m, ramFile);
    mountDisk(m, diskPath, diskFile);
    ecaLoaderRegisterHooks(m);
    printf("Loaded state: reg='%s', RAM='%s', PC=%04X\n", regPath, ramPath, m->reg.pc);
  } else {
    // process a PRG file
    const char* path = "../as/EmuTestAdd.prg";
    word_t overrideAddr = 0x0810;
    bool useFileAddress = false;
    if (argc > 1) {
      path = argv[1];
      useFileAddress = true;
    }
    if (argc > 2) {
      overrideAddr = parseAddr(argv[2]);
      useFileAddress = false;
    }
    if (argc > 3) {
      fprintf(stderr, "Too many arguments.\n");
      exit(1);
    }
    buf_t* prg = readFile(path);
    word_t fileAddr = loadPRG(m, prg);
    if (useFileAddress)
      m->reg.pc = fileAddr;
    else
      m->reg.pc = overrideAddr;
    printf("Loaded file '%s', starting at $%04X\n", path, m->reg.pc);
  }
  interp(m);
  int million = m->reg.ic / 1000000;
  printf("Exit: PC=%X, IC="IC_FMT" (%d million)\n", m->reg.pc, m->reg.ic, million);
  dumpRam(m, "ramdump.bin");
}

