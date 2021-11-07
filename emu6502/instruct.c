
#include "em.h"

const char* instructionMnemonics[] = {
  "<X>", // there is no instruction 0
  "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK",
  "BVC", "BVS", "CLC", "CLD", "CLI", "CLV", "CMP", "CPX", "CPY", "DEC", "DEX",
  "DEY", "EOR", "INC", "INX", "INY", "JMP", "JSR", "LDA", "LDX", "LDY", "LSR",
  "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL", "ROR", "RTI", "RTS", "SBC",
  "SEC", "SED", "SEI", "STA", "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS",
  "TYA",
};

AddrModeInfo addrModeInfo[] = {
  { "impl",   0 },
  { "imm",    0 },
  { "<X>",    0 }, // gap in numbering here
  { "zpg",    AMF_Resolve|AMF_Zpg|AMF_NoIndex },
  { "zpg,X",  AMF_Resolve|AMF_Zpg|AMF_X       },
  { "zpg,Y",  AMF_Resolve|AMF_Zpg|AMF_Y       },
  { "rel",    AMF_Resolve        |AMF_NoIndex },
  { "abs",    AMF_Resolve|AMF_Abs|AMF_NoIndex },
  { "abs,X",  AMF_Resolve|AMF_Abs|AMF_X       },
  { "abs,Y",  AMF_Resolve|AMF_Abs|AMF_Y       },
  { "ind",    AMF_Resolve|AMF_Ind|AMF_NoIndex },
  { "X,ind",  AMF_Resolve|AMF_Ind|AMF_X       },
  { "ind,Y",  AMF_Resolve|AMF_Ind|AMF_Y       },
};

// Full names of the addressing modes.
const char* addressModeLongNames[] = {
  "implied",
  "immediate",
  "A", // unused
  "zero-page",
  "zero-page, X-indexed",
  "zero-page, Y-indexed",
  "relative",
  "absolute",
  "absolute, X-indexed",
  "absolute, Y-indexed",
  "indirect",
  "X-indexed, indirect",
  "indirect, Y-indexed",
};

// Standard mnemonics for the addressing modes.
const char* addressModeNames[] = {
  "impl",
  "imm",
  "A", // unused
  "zpg",
  "zpg,X",
  "zpg,Y",
  "rel",
  "abs",
  "abs,X",
  "abs,Y",
  "ind",
  "X,ind",
  "ind,Y",
};

// Very short abbreviations for the addressing modes.
const char* addressMode2charNames[] = {
  "il",
  "im",
  "ra", // unused
  "zp",
  "zx",
  "zy",
  "re",
  "ab",
  "ax",
  "ay",
  "in",
  "xi",
  "iy",
};

instruction_t instructionSet[0x100] = {
#include "instrdef.inc"
};

const char* c64RomErrors[] = {
  "<NONE>",
  "TOO MANY FILES",
  "FILE OPEN",
  "FILE NOT OPEN",
  "FILE NOT FOUND",
  "DEVICE NOT PRESENT",
  "NOT INPUT FILE",
  "NOT OUTPUT FILE",
  "MISSING FILE NAME",
  "BAD DEVICE #",
};

