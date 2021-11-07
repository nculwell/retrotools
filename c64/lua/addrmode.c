
struct registers {
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint16_t s;
  uint16_t pc;
  uint8_t p;
};

enum instructions {
  ADC=1, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD,
  CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA,
  LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC, SEC,
  SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA,
  INSTRUCTION_COUNT
};

  AMF_Resolve = 0x80,
  AMF_TwoByte = 0x40,
  AMF_Ind     = 0x20,
  AMF_Rel     = 0x20,
};
enum addrModes {

  AM_impl = 0x00,
  AM_imm  = 0x01,
  AM_A    = 0x00,

  AM_zpg, = AMF_Resolve | 0 | 0x01,
  AM_zpgX = AMF_Resolve | 0 | 0x42,
  AM_zpgY = 0x44,

  AM_abs  = 0x21,
  AM_absX = 0x22,
  AM_absY = 0x24,

  AM_ind  = 0x11,
  AM_Xind = 0x12,
  AM_indY = 0x14,

  AM_rel  = 0x01,

};
typedef uint8_t instructionSet_t[0x100][2];
