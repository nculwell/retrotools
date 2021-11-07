
#define _DEFAULT_SOURCE
#define _SVID_SOURCE

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "em.h"
#include "emtrace.h"

#include <stdlib.h>

typedef struct {
  int addr;
  const char* name;
} LoaderLabel;

LoaderLabel LABELS[] = {
  // Labels are defined in another file to reduce clutter. The terminating
  // record is here, though, so the other file can contain just real data.
#include "ecalabels.c"
  { -1, 0 } // label list terminator
};

// Table of bytecodes at $C533
// At left are the bytecodes found in the source (00, 01, etc.) and the
// addresses of the code that implements each instruction.

// When reading a single byte from the stream, XOR it with $6B before using.
// When reading a word, XOR the low byte with $2D and the high byte with $29.
// Instruction "XOR_pC_A" (0F, at C628) is complicated, see it for description.

// I've named the working locations as "registers", although only "A" is definitely used like a typical register (it's the accumulator).
// A = $28
// B = $2A:2B
// C = $2C:2D (used as pointer to memory)
// D = $2E:2F
// X = $C6C1 (called this because native subroutines pass it in X)
// Y = $C6C2 (called this because native subroutines pass it in Y)

#define INSTRUCTION_COUNT 0x14  // length of the instruction list

const char* LOADER_BYTECODE_NAMES[] = {
  /* 00 C547 */ "GOTO",                   // word operand (C56D): address of next instruction
  /* 01 C583 */ "AND_IMM_BYTE_WITH_A",    // byte operand (C65F): AND with A and store in A
  /* 02 C5D9 */ "GOSUB",                  // word operand (C56D): address to set IP to (save IP on stack first)
  /* 03 C586 */ "FUNCALL_NATIVE",         // word operand (C56D): JSR to operand; pass A,X,Y in hardware regs A,X,Y; save hw A to A after return
  /* 04 C5AE */ "STORE_IMM_BYTE_IN_A",    // byte operand (C5B7): store value A
  /* 05 C5BC */ "STORE_VAR_TO_A",         // word operand (C56D): pointer to var, copy var value to A
  /* 06 C5A4 */ "IF_A_ZERO_THEN_GOTO",    // word operand (C56D): set IP to operand if A=0
  /* 07 C672 */ "STORE_A_TO_VAR",         // word operand (C56D): copy A to ptr addr
  /* 08 C60D */ "SUB_IMM_BYTE_FROM_A",    // byte operand (C618): subtract immediate value from A, store to A
  /* 09 C59E */ "GOTO NATIVE",            // word operand (C56D): JMP to operand
  /* 0A C5F1 */ "RETURN",                 // no operand: pull IP off of stack
  /* 0B C5C8 */ "STORE_ARRAY_ELT_TO_A",   // word operand (C56D): op is array base, interp reg A is offset, look up byte and store it in A
  /* 0C C608 */ "ASL_A",                  // no operand: ASL interp register A
  /* 0D C622 */ "ADD_1_TO_VAR",           // word operand (C56D): interpret op as addr, load byte from there, add 1 and store back to var and in A
  /* 0E C625 */ "ADD_VAR_TO_A",           // word operand (C56D): interpret op as addr, load byte from there, add A and store back to A (but not to var)
  /* 0F C628 */ "XOR_pC_A", /* SCRAM */   // no operand:
                                          //   * load the byte that C points to (*C), XOR it with A, store the result back to *C
                                          //   * increment C and do this again at the next location
                                          //   * increment C again, with carry to high byte of pointer this time
                                          //   * XOR the high byte of C with $7F and store the result in A
  /* 10 C62B */ "IF_A_NZ_THEN_GOTO",      // word operand (C56D): if A is nonzero then set IP to operand
  /* 11 C62E */ "SUB_VAR_FROM_A",         // word operand (C56D): subtract variable at op from A, store result to A
  /* 12 C631 */ "IF_A_Z_OR_POS_GOTO",     // word operand (C56D): if A>=0 then set IP to operand
  /* 13 C634 */ "STORE_XY",               // word operand (C56D): store operand lo:hi to X:Y
};

// Nicer set of mnemonics for disassembly
const char* MNEMONICS[] = {
  "GOTO",
  "AND",
  "GOSUB",
  "JSR",
  "LDA", // imm
  "LDA", // var
  "GOTOZ", // goto if zero
  "STA", // var
  "SUB",
  "JMP",
  "RET",
  "ARR",
  "ASL",
  "INC",
  "ADD",
  "DCRACW", // "decrypt word at *C using A"
  "GOTONZ",
  "SUB",
  "GOTOGE",
  "LDXY",
};

// IP = instruction pointer, stored at $26:27
// IP advanced = the IP already moved past this data so you need to subtract
//  from it to see where the data came from

#define BYTECODE_BASE_ADDRESS 0xC533      // table of bytecode implementation offsets; shouldn't be needed

#define INTERP_BYTECODE_POSTREAD 0xC51A   // can read instruction from A here
#define INTERP_BYTECODE_PREJUMP  0xC530   // can read instruction dest from $C531:C532 here; IP advanced

#define INTERP_OPERAND_WORD_READY 0xC56D  // can read word operand from $22:23 here; IP has advanced already
#define INTERP_OPERAND_BYTE_OP_1  0XC65F
#define INTERP_OPERAND_BYTE_OP_2  0XC5B7
#define INTERP_OPERAND_BYTE_OP_3  0xC618
#define INTERP_EXIT_BITSPREAD     0xC7AA
#define INTERP_UNTLK              0xC1F7

enum {
  LDRHOOK_ID_RANGE_START = 0x100,

  LDRHOOK_READ_INSTR,
  LDRHOOK_CAPTURE_WORDOP,
  LDRHOOK_CAPTURE_BYTEOP,

  LDRHOOK_RAMDUMP,

  LDRHOOK_ID_RANGE_END,
};

struct LoaderInterpHook {
  int pc;
  int id;
  const char* name;
};

struct LoaderInterpHook LOADER_INTERP_HOOKS[] = {
  { INTERP_BYTECODE_POSTREAD, LDRHOOK_READ_INSTR, "Read bytecode" },
  { INTERP_OPERAND_WORD_READY, LDRHOOK_CAPTURE_WORDOP, "Capture word operand" },
  { INTERP_OPERAND_BYTE_OP_1, LDRHOOK_CAPTURE_BYTEOP, "Capture byte operand 1" },
  { INTERP_OPERAND_BYTE_OP_2, LDRHOOK_CAPTURE_BYTEOP, "Capture byte operand 2" },
  { INTERP_OPERAND_BYTE_OP_3, LDRHOOK_CAPTURE_BYTEOP, "Capture byte operand 3" },
  { INTERP_EXIT_BITSPREAD, LDRHOOK_RAMDUMP, "Bit Spread routine" },
  { INTERP_UNTLK, LDRHOOK_RAMDUMP, "UNTLK" },
  { 0, 0, 0 },
};

enum {
  BC_ATOM_NONE,
  BC_ATOM_INST, BC_ATOM_BYTE, BC_ATOM_WORD_LO, BC_ATOM_WORD_HI,
};

typedef struct {
  byte_t type;
  byte_t value;
  byte_t raw;
} LoaderBytecodeAtom;

typedef struct {
  LoaderBytecodeAtom atoms[RAM_SIZE];
} LoaderBytecodeDisassembly;

typedef struct {
  byte_t instructionBytecode;
  word_t instructionAddr;
  LoaderBytecodeDisassembly disassembly;
  Emu* m;
  int ramDumpCount;
} LoaderHookPrivateData;

void ecaLoaderHookCallback(Emu* m, int pc, ExecutionHook* hook) {
  LoaderHookPrivateData* privateData = hook->privateData;
  assert(privateData);
  trace(m, true, "LOADER HOOK CALLBACK AT %04X, ID %X: %s", pc, hook->hookID, hook->name);
  int hookID = hook->hookID;
  word_t ip = toWord(RAM[0x26], RAM[0x27]) + Y;
  bool changed = false;
  const char* changeText = " CHANGED!";
  // TODO: Dispatch on ID, not PC
  switch (hookID) {
    case LDRHOOK_READ_INSTR:
      {
        byte_t byc = RAM[ip];
        assert(byc < INSTRUCTION_COUNT);
        privateData->instructionBytecode = byc;
        privateData->instructionAddr = ip;
        LoaderBytecodeAtom* atom = &(privateData->disassembly.atoms[ip]);
        if (atom->type != BC_ATOM_NONE)
          if (atom->type != BC_ATOM_INST || atom->value != byc)
            changed = true;
        trace(m, true, "LOADER HOOK: IP=%04X inst %02X '%s'%s", ip, byc, MNEMONICS[byc], changed ? changeText : "");
        atom->type = BC_ATOM_INST;
        atom->value = byc;
        atom->raw = byc;
      }
      break;
    case LDRHOOK_CAPTURE_WORDOP:
      {
        ip -= 2; // IP advances before we read a word operand, adjust for that
        word_t op = toWord(RAM[0x22], RAM[0x23]);
        LoaderBytecodeAtom* atom = &(privateData->disassembly.atoms[ip]);
        LoaderBytecodeAtom* atom2 = &(privateData->disassembly.atoms[ip+1]);
        if (atom->type != BC_ATOM_NONE) {
          if (atom->type != BC_ATOM_WORD_LO || atom->value != RAM[0x22])
            changed = true;
          if (atom2->type != BC_ATOM_WORD_HI || atom2->value != RAM[0x23])
            changed = true;
        }
        trace(m, true, "LOADER HOOK: IP=%04X word %04X (inst %02X at IP=%04X)%s", ip, op, privateData->instructionBytecode, privateData->instructionAddr, changed ? changeText : "");
        atom->type = BC_ATOM_WORD_LO;
        atom->value = RAM[0x22];
        atom->raw = RAM[ip];
        atom2->type = BC_ATOM_WORD_HI;
        atom2->value = RAM[0x23];
        atom2->raw = RAM[ip+1];
      }
      break;
    case LDRHOOK_CAPTURE_BYTEOP:
      {
        ip--; // IP advances before we read a byte operand, adjust for that
        word_t op = A;
        LoaderBytecodeAtom* atom = &(privateData->disassembly.atoms[ip]);
        if (atom->type != BC_ATOM_NONE)
          if (atom->type != BC_ATOM_BYTE || atom->value != op)
            changed = true;
        trace(m, true, "LOADER HOOK: IP=%04X byte %02X (inst %02X at IP=%04X)%s", ip, op, privateData->instructionBytecode, privateData->instructionAddr, changed ? changeText : "");
        atom->type = BC_ATOM_BYTE;
        atom->value = op;
        atom->raw = RAM[ip];
      }
      break;
    case LDRHOOK_RAMDUMP:
      {
        int ramDumpNumber = ++privateData->ramDumpCount;
        if (ramDumpNumber < 1000) {
          char filename[32];
          sprintf(filename, "RamDump%03d_%04X.bin", ramDumpNumber, pc);
          dumpRam(m, filename);
        }
      }
      break;
    default:
      error(m, "UNEXPECTED HOOK: PC=%04X ID=%X \"%s\"", pc, hook->hookID, hook->name);
  }
}

int findLabel(int labelAddr) {
  // Linear search. Could sort these and do a binary search but it's probably
  // not worth the effort.
  int labelIndex = 0;
  for (; LABELS[labelIndex].addr != -1; labelIndex++) {
    if (LABELS[labelIndex].addr == labelAddr)
      return labelIndex;
  }
  return -1;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void saveDisassembly(int exitCode, void* data) {
  LoaderHookPrivateData* privateData = data;
  //Emu* m = privateData->m;
  LoaderBytecodeAtom *dis = privateData->disassembly.atoms;
  // Calculate the max length of a mnemonic and fill in the formst string
  // correctly.
  int maxMnemonicLen = 0;
  for (int i=0; i < INSTRUCTION_COUNT; i++) {
    int len = strlen(MNEMONICS[i]);
    if (len > maxMnemonicLen)
      maxMnemonicLen = len;
  }
  // Open the output file.
  FILE* f = fopen("ldr_disassembly.txt", "wt");
  if (!f) {
    fprintf(stderr, "Unable to open disassembly file.\n");
    return;
  }
  bool prevWasEmpty = true;
  for (int ip=0; ip < RAM_SIZE; ip++) {
    if (dis[ip].type == BC_ATOM_NONE) {
      if (!prevWasEmpty)
        fprintf(f, "\n");
      prevWasEmpty = true;
    } else {
      if (dis[ip].type == BC_ATOM_INST) {
        byte_t inst = dis[ip].value;
        assert(inst < INSTRUCTION_COUNT);
        // Print label for this line, if it has one
        int lineLabelIndex = findLabel(ip);
        if (lineLabelIndex != -1) {
          fprintf(f, "; %s\n", LABELS[lineLabelIndex].name);
        }
        // Start printing code line
        const char* mnemonic = MNEMONICS[inst];
        fprintf(f, "%04X  ", ip);
        fprintf(f, "%02X ", dis[ip].raw);
        if (dis[ip+1].type == BC_ATOM_BYTE) {
          // byte operand
          ip++;
          byte_t b = dis[ip].value;
          fprintf(f, "%02X ", dis[ip].raw);
          fprintf(f, "   ");
          fprintf(f, " %s", mnemonic);
          for (int c=strlen(mnemonic); c < maxMnemonicLen; c++)
            fprintf(f, " ");
          fprintf(f, " #$%02X", b);
          if (isprint(b))
            fprintf(f, " '%c'", b);
          fprintf(f, "\n");
        } else if (dis[ip+1].type == BC_ATOM_WORD_LO) {
          // word operand
          ip++;
          byte_t wl = dis[ip].value;
          fprintf(f, "%02X ", dis[ip].raw);
          if (dis[ip+1].type != BC_ATOM_WORD_HI) {
            fprintf(f, "   ");
            fprintf(f, " INCOMPLETE WORD\n");
          } else {
            ip++;
            byte_t wh = dis[ip].value;
            word_t wordValue = toWord(wl, wh);
            fprintf(f, "%02X ", dis[ip].raw);
            fprintf(f, " %s", mnemonic);
            for (int c=strlen(mnemonic); c < maxMnemonicLen; c++)
              fprintf(f, " ");
            int operandLabelIndex = findLabel(wordValue);
            if (inst == 0x13) // LDXY
              fprintf(f, " #$%04X (X=#$%02X, Y=#$%02X)", wordValue, wl, wh);
            else
              fprintf(f, " $%04X", wordValue);
            if (operandLabelIndex != -1)
              fprintf(f, " '%s'", LABELS[operandLabelIndex].name);
            fprintf(f, "\n");
          }
        } else {
          // no operand
          fprintf(f, "   ");
          fprintf(f, "   ");
          fprintf(f, " %s\n", mnemonic);
        }
      } else {
        fprintf(f, "   ");
        fprintf(f, "   ");
        fprintf(f, " STRAY BYTE\n");
      }
      prevWasEmpty = false;
    }
  }
  fclose(f);
}

void ecaLoaderRegisterHooks(Emu* m) {
  ExecutionHook hook = { 0 };
  LoaderHookPrivateData* privateData = malloc(sizeof(LoaderHookPrivateData));
  assert(privateData);
  privateData->m = m;
  for (int i=0; LOADER_INTERP_HOOKS[i].pc != 0; i++) {
    hook = (ExecutionHook){
      .pcHookAddress = LOADER_INTERP_HOOKS[i].pc,
      .hookType = HOOKTYPE_EXEC,
      .isPostHook = false,
      .hookID = LOADER_INTERP_HOOKS[i].id,
      .name = LOADER_INTERP_HOOKS[i].name,
      .callback = ecaLoaderHookCallback,
      .privateData = privateData,
    };
    registerHook(m, &hook);
  }
  on_exit(saveDisassembly, privateData);
};


