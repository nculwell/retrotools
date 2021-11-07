
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>

#include "em.h"
#include "emtrace.h"

// STRING BUILDING HELPERS

static char hexDigits[] = "0123456789ABCDEF";

static inline char* putHexByte(char* s, byte_t b) {
  s[0] = hexDigits[b >> 4];
  s[1] = hexDigits[b & 0x0F];
  return s+2;
}

// This is basically strcpy except it doesn't append 0 and it returns a pointer
// to the end of where it copied the string (so you can keep adding to it).
static inline char* putString(char* s, const char* str) {
  while (*str) {
    *(s++) = *(str++);
  }
  return s;
}

static inline char* alignToColumn(char* strInsertionPoint, char* strStart, int column) {
  while (strInsertionPoint < strStart + column)
    *(strInsertionPoint++) = ' ';
  return strInsertionPoint;
}

static inline char* spc2(char* s) {
  *(s++)=' ';
  *(s++)=' ';
  return s;
}

static inline char* spc3(char* s) {
  *(s++)=' ';
  *(s++)=' ';
  *(s++)=' ';
  return s;
}

static inline char* spc4(char* s) {
  *(s++)=' ';
  *(s++)=' ';
  *(s++)=' ';
  *(s++)=' ';
  return s;
}

// TRACE OUTPUT

void error(emu_t* m, const char* fmt, ...) {
  FILE* f;
  if (!m || !m->traceFile)
    f = stderr;
  else
    f = m->traceFile;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  putc('\n', f);
  fflush(f);
  exit(1);
}

#if TRACE_ON
void trace(emu_t* m, bool indent, const char* fmt, ...) {
  FILE* f = m->traceFile;
  if (f == NULL)
    return;
  if (indent) {
#ifdef TRACE_EXTRA
    for (int i=0; i < 21; i++)
      putc(' ', f);
#else
    return; // all indented lines are extra trace info
#endif
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  putc('\n', f);
}
#endif

#ifdef TRACE_EXTRA
void traceSetPC(emu_t* m, word_t toAddr) {
  trace(m, true, "SET PC: %04X -> %04X", m->reg.pc, toAddr);
}
#endif

#ifdef TRACE_EXTRA
void traceStack(emu_t* m, byte_t affectedByte, char sepChar) {
  char buf[PRINT_STACK_BUFSIZ];
  char* p = buf;
  p = putHexByte(p, affectedByte);
  *(p++) = ' ';
  *(p++) = sepChar;
  *(p++) = sepChar;
  word_t top = 0x101 + m->reg.s; // top of the stack
  word_t end = 0x0200; // end of bytes to print (max stack addr + 1)
  word_t limit = top + PRINT_STACK_MAX_BYTES;
  if (limit < end)
    end = limit;
  for (word_t addr = top; addr < end; addr++) {
    *(p++) = ' ';
    p = putHexByte(p, RAM[addr]);
  }
  if (end == 0x200) {
    p = putString(p, " ||");
  } else {
    p = putString(p, "...");
  }
  *p = 0;
  trace(m, true, buf);
}
#endif

#ifndef TRACE_EXTRA
#pragma GCC diagnostic pop
#endif

// IMPLEMENTATIONS OF INDIVIDUAL OPERATIONS

static void setNZ(emu_t* m, byte_t byteValue) {
  if (byteValue == 0) {
    setFlag(m, FLAG_Z, true);
    setFlag(m, FLAG_N, false);
  } else {
    setFlag(m, FLAG_Z, false);
    setFlag(m, FLAG_N, byteValue & 0x80);
  }
}

static void push(emu_t* m, byte_t operand) {
  if (SP == 0)
    error(m, "Stack overflow.");
  traceStack(m, operand, '>');
  RAM[0x100 + SP] = operand;
  SP--;
}

static byte_t pull(emu_t* m) {
  if (SP == 0xFF)
    error(m, "Stack underflow.");
  SP++;
  byte_t v = RAM[0x100 + SP];
  setNZ(m, v);
  traceStack(m, v, '<');
  return v;
}

// C64 banks:
// %x00: RAM visible in all three areas.
// %x01: RAM visible at $A000-$BFFF and $E000-$FFFF.
// %x10: RAM visible at $A000-$BFFF; KERNAL ROM visible at $E000-$FFFF.
// %x11: BASIC ROM visible at $A000-$BFFF; KERNAL ROM visible at $E000-$FFFF.
// %0xx: Character ROM visible at $D000-$DFFF. (Except for the value %000, see above.)
// %1xx: I/O area visible at $D000-$DFFF. (Except for the value %100, see above.)
byte_t loadBanked(Emu* m, word_t addr) {

  // Macros to make it easier to define the banks.
  // Note that this difference is made unsigned but the way it wraps around
  // should ensure the correctness of the assert.
  // The pragmas are here so we don't get warnings on the check for addr <=
  // 0XFFFF, which is always true, and so GCC won't complain about array bounds
  // (which we've checked).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Warray-bounds"
#define ROM_RANGE(RANGE_NAME, BEGIN, END) \
  if (BEGIN <= addr && addr <= END) { \
    word_t offset = addr - BEGIN; \
    assert(offset < sizeof(m->rom.RANGE_NAME)); \
    return m->rom.RANGE_NAME[offset - BEGIN]; \
  }
#define CHARACTER_ROM_VISIBLE   ROM_RANGE(chargen,  0xD000, 0xDFFF)
#define BASIC_ROM_VISIBLE       ROM_RANGE(basic,    0xA000, 0xBFFF)
#define KERNAL_ROM_VISIBLE      ROM_RANGE(kernal,   0xE000, 0xFFFF)
/*
  if (0xD000 <= addr && addr <= 0xDFFF) return m->rom.chargen[addr - 0xD000]
  if (0xA000 <= addr && addr <= 0xBFFF) return m->rom.basic[addr - 0xA000]
  if (0xE000 <= addr && addr <= 0xFFFF) return m->rom.kernal[addr - 0xE000]
*/

  // Switch block defines bank behavior.
  // Note that we're not emulating I/O, so reads from the I/O area are treated
  // as reads from RAM. We're also not emulating the EXROM or GAME pins.
  switch (RAM[0x0001] & 0b111) {

    case 0b000:
      break;
    case 0b001:
      CHARACTER_ROM_VISIBLE;
      break;
    case 0b010:
      CHARACTER_ROM_VISIBLE;
      KERNAL_ROM_VISIBLE;
      break;
    case 0b011:
      CHARACTER_ROM_VISIBLE;
      BASIC_ROM_VISIBLE;
      KERNAL_ROM_VISIBLE;
      break;

    case 0b100:
      break;
    case 0b101:
      // I/O at $D000-$DFFF
      break;
    case 0b110:
      // I/O at $D000-$DFFF
      KERNAL_ROM_VISIBLE;
      break;
    case 0b111:
      // I/O at $D000-$DFFF
      BASIC_ROM_VISIBLE;
      KERNAL_ROM_VISIBLE;
      break;

  }
  return RAM[addr];

#undef KERNAL_ROM_VISIBLE
#undef BASIC_ROM_VISIBLE
#undef CHARACTER_ROM_VISIBLE
#undef ROM_RANGE
#pragma GCC diagnostic pop
}

byte_t load(emu_t* m, word_t addr) {
  byte_t value = loadBanked(m, addr);
  trace(m, true, "LOAD %04X: %02X", addr, value);
  return value;
}

byte_t store(emu_t* m, byte_t value, word_t addr) {
  trace(m, true, "STORE %04X: %02X -> %02X", addr, m->ram[addr], value);
  m->ram[addr] = value;
  return value;
}

static byte_t bitwiseASL(emu_t* m, byte_t value) {
  setFlag(m, FLAG_C, value & 0x80);
  value <<= 1;
  setNZ(m, value);
  return value;
}

static byte_t bitwiseLSR(emu_t* m, byte_t value) {
  setFlag(m, FLAG_C, value & 1);
  value >>= 1;
  setNZ(m, value);
  return value;
}

static byte_t bitwiseROL(emu_t* m, byte_t value) {
  bool carrySetBefore = getFlag(m, FLAG_C);
  setFlag(m, FLAG_C, value & 0x80);
  value <<= 1;
  if (carrySetBefore)
    value++; // set the low bit to 1
  setNZ(m, value);
  return value;
}

static byte_t bitwiseROR(emu_t* m, byte_t value) {
  bool carrySetBefore = getFlag(m, FLAG_C);
  setFlag(m, FLAG_C, value & 1);
  value >>= 1;
  if (carrySetBefore)
    value |= 0x80; // set the high bit to 1
  setNZ(m, value);
  return value;
}

static void add(emu_t* m, byte_t regVal, byte_t memVal, bool isCmp) {
  word_t diff = regVal + memVal;
  // The carry flag always means +1 here.
  // This works for subtraction because we're subtracting the one's complement
  // instead of the two's complement. (The +1 from the carry flag is the
  // missing +1 to convert one's complement to two's complement.)
  if (isCmp || getFlag(m, FLAG_C))
    diff++; // CMP behaves like SBC with carry set
  byte_t b = diff;
  // All instructions set N, Z and C.
  setNZ(m, b);
  setFlag(m, FLAG_C, diff & 0x100);
  if (!isCmp) {
    // ADC and SBC set A and V, but compare instructions don't.
    bool overflow = (regVal ^ b) & (memVal ^ b) & 0x80;
    setFlag(m, FLAG_V, overflow);
    A = b;
  }
}

static void returnFromSub(emu_t* m) {
  word_t returnAddr = pull(m);
  returnAddr |= pull(m) << 8;
  returnAddr++; // correct for how JSR pushes addresses
  traceSetPC(m, returnAddr);
  m->reg.pc = returnAddr;
}

static void jump(emu_t* m, word_t addr, bool far) {
  traceSetPC(m, addr);
  if (far && addr >= 0xF000) {
    emulateC64ROM(m, addr);
    returnFromSub(m);
  } else {
    m->reg.pc = addr;
  }
}

// RESOLVE ADDRESSING MODES

static inline word_t deref(emu_t* m, word_t pointer) {
  return toWord(RAM[pointer], RAM[pointer+1]);
}

static bool resolveAddress(
    emu_t* m,
    byte_t admd,
    AddrModeFlags_t admdFlags,
    word_t* effAddr,
    word_t* rawAddr)
{
  word_t addr = RAM[PC++];
  *rawAddr = addr;
  if (admdFlags & AMF_Abs) {
    // Read second byte.
    addr |= RAM[PC++] << 8;
    *rawAddr = addr;
    // absolute
    if (admd == AM_absX)
      addr += X;
    else if (admd == AM_absY)
      addr += Y;
    else
      assert(admd == AM_abs);
  } else {
    if (admdFlags & AMF_Ind) {
      // indirect
      if (admd == AM_Xind) {
        addr = deref(m, addr + X);
      } else {
        if (admd == AM_indY) {
          addr = deref(m, addr);
          addr += Y;
        } else {
          assert(admd == AM_ind);
          addr |= RAM[PC++] << 8;
          *rawAddr = addr;
          addr = deref(m, addr);
        }
      }
      *effAddr = addr;
      return true;
    } else if (admd == AM_rel) {
      // relative: operand is a signed offset
      addr = PC + ((int8_t)addr);
    } else {
      // zero page
      if (!(admdFlags & AMF_Zpg)) { fprintf(stderr, "%s\n", addrModeInfo[admd].name); }
      assert(admdFlags & AMF_Zpg);
      if (admd == AM_zpgX)
        addr += X;
      else if (admd == AM_zpgY)
        addr += Y;
      else
        assert(admd == AM_zpg);
    }
  }
  *effAddr = addr;
  return false;
}

// IMPLEMENTATIONS OF GROUPS OF INSTRUCTIONS

static void interpImm(emu_t* m, byte_t inst, byte_t operand) {
  switch (inst) {

    // LOAD

    case LDA:
      m->reg.a = operand;
      setNZ(m, m->reg.a);
      break;
    case LDX:
      m->reg.x = operand;
      setNZ(m, m->reg.x);
      break;
    case LDY:
      m->reg.y = operand;
      setNZ(m, m->reg.y);
      break;

      // ADD / SUB

    case ADC:
      add(m, m->reg.a, operand, false);
      break;
    case SBC:
      add(m, m->reg.a, ~operand, false);
      break;
    case CMP:
      add(m, m->reg.a, ~operand, true);
      break;
    case CPY:
      add(m, m->reg.y, ~operand, true);
      break;
    case CPX:
      add(m, m->reg.x, ~operand, true);
      break;


      // BITWISE

    case ORA:
      A |= operand;
      setNZ(m, A);
      break;
    case AND:
      A &= operand;
      setNZ(m, A);
      break;
    case EOR:
      A ^= operand;
      setNZ(m, A);
      break;

    default:
      error(m, "%s:%d: Unexpected instruction: %s (PC=%04X, IC=" IC_FMT ")",
          __FILE__, __LINE__,
          instructionMnemonics[inst], m->reg.pc, m->reg.ic);
  }
}

static void interpAddr(emu_t* m, byte_t inst, word_t addr) {
  assert(m);
  assert(inst);

  switch (inst) {

    // JUMPS

    case JSR:
      {
        word_t pushAddr = m->reg.pc - 1;
        push(m, toHi(pushAddr));
        push(m, toLo(pushAddr));
        jump(m, addr, true);
      }
      break;
    case JMP:
      jump(m, addr, true);
      break;

      // BRANCHES

    case BPL:
      if (!getFlag(m, FLAG_N))
        jump(m, addr, false);
      break;
    case BMI:
      if (getFlag(m, FLAG_N))
        jump(m, addr, false);
      break;
    case BVS:
      if (getFlag(m, FLAG_V))
        jump(m, addr, false);
      break;
    case BCC:
      if (!getFlag(m, FLAG_C))
        jump(m, addr, false);
      break;
    case BCS:
      if (getFlag(m, FLAG_C))
        jump(m, addr, false);
      break;
    case BNE:
      if (!getFlag(m, FLAG_Z))
        jump(m, addr, false);
      break;
    case BEQ:
      if (getFlag(m, FLAG_Z))
        jump(m, addr, false);
      break;

      // LOAD

    case LDA:
      m->reg.a = load(m, addr);
      setNZ(m, m->reg.a);
      break;
    case LDX:
      m->reg.x = load(m, addr);
      setNZ(m, m->reg.x);
      break;
    case LDY:
      m->reg.y = load(m, addr);
      setNZ(m, m->reg.y);
      break;

      // STORE

    case STA:
      store(m, m->reg.a, addr);
      break;
    case STX:
      store(m, m->reg.x, addr);
      break;
    case STY:
      store(m, m->reg.y, addr);
      break;
    case BIT:
      store(m, m->reg.p, addr);
      break;

      // INCREMENT / DECREMENT

    case INC:
      {
        byte_t v = store(m, RAM[addr] + 1, addr);
        setNZ(m, v);
      }
      break;
    case DEC:
      {
        byte_t v = store(m, RAM[addr] + 1, addr);
        setNZ(m, v);
      }
      break;

      // BIT SHIFTS

    case ASL:
        m->ram[addr] = bitwiseASL(m, m->ram[addr]);
        break;
    case LSR:
        m->ram[addr] = bitwiseLSR(m, m->ram[addr]);
        break;
    case ROL:
        m->ram[addr] = bitwiseROL(m, m->ram[addr]);
        break;
    case ROR:
        m->ram[addr] = bitwiseROR(m, m->ram[addr]);
        break;

    default:
        // The opcodes with immediate arguments can be applied to memory just
        // by loading the value from memory and calling the same code.
        interpImm(m, inst, m->ram[addr]);

  }
}

static void interpImpl(emu_t* m, byte_t inst) {
  switch (inst) {

    // RETURN

    case RTS:
      if (m->reg.s > 0xFD)
        error(m, "Stack underflow in RTS.");
      returnFromSub(m);
      break;
      
      // STACK

    case PHP:
      push(m, m->reg.p);
      break;
    case PLP:
      m->reg.p = pull(m);
      break;
    case PHA:
      push(m, m->reg.a);
      break;
    case PLA:
      m->reg.a = pull(m);
      break;

      // FLAGS

    case CLC:
      setFlag(m, FLAG_C, false);
      break;
    case SEC:
      setFlag(m, FLAG_C, true);
      break;
    case CLV:
      setFlag(m, FLAG_V, false);
      break;
    case CLD:
      setFlag(m, FLAG_D, false);
      break;
    case SED:
      setFlag(m, FLAG_D, true);
      break;
    case CLI:
      setFlag(m, FLAG_I, false);
      break;
    case SEI:
      setFlag(m, FLAG_I, true);
      break;

      // INCREMENT / DECREMENT

    case INX:
      m->reg.x = m->reg.x + 1;
      setNZ(m, m->reg.x);
      break;
    case DEX:
      m->reg.x = m->reg.x - 1;
      setNZ(m, m->reg.x);
      break;
    case INY:
      m->reg.y = m->reg.y + 1;
      setNZ(m, m->reg.y);
      break;
    case DEY:
      m->reg.y = m->reg.y - 1;
      setNZ(m, m->reg.y);
      break;

      // TRANSFER REGISTERS

    case TYA:
      m->reg.a = m->reg.y;
      setNZ(m, m->reg.a);
      break;
    case TAY:
      m->reg.y = m->reg.a;
      setNZ(m, m->reg.y);
      break;
    case TXA:
      m->reg.a = m->reg.x;
      setNZ(m, m->reg.a);
      break;
    case TAX:
      m->reg.x = m->reg.a;
      setNZ(m, m->reg.a);
      break;
    case TXS:
      m->reg.s = m->reg.x;
      // This instruction doesn't set N/Z
      break;
    case TSX:
      m->reg.x = m->reg.s;
      setNZ(m, m->reg.a);
      break;

      // BIT SHIFTS

    case ASL:
        m->reg.a = bitwiseASL(m, m->reg.a);
        break;
    case LSR:
        m->reg.a = bitwiseLSR(m, m->reg.a);
        break;
    case ROL:
        m->reg.a = bitwiseROL(m, m->reg.a);
        break;
    case ROR:
        m->reg.a = bitwiseROR(m, m->reg.a);
        break;

    case NOP:
        // do nothing
        break;

      // Instructions that are not supported because we're not emulating
      // interrupts.

    case BRK:
    case RTI:
      error(m, "Interrupt-related opcodes not supported.");

    default:
      error(m, "%s:%d: Unexpected instruction: %s (PC=%04X, IC=" IC_FMT ")",
          __FILE__, __LINE__,
          instructionMnemonics[inst], PC, m->reg.ic);
  }
}

#if TRACE_ON
static void traceInstruction(
    emu_t* m,
    word_t opcodeAddr,
    byte_t inst,
    byte_t admd,
    AddrModeFlags_t admdFlags,
    word_t operand,
    word_t rawAddr)
{
  char buf[TRACE_BUFSIZ];
  char* s = buf;
  // Prefix
  *(s++) = '.';
  *(s++) = 'C';
  *(s++) = ':';
  // Address
  s = putHexByte(s, toHi(opcodeAddr));
  s = putHexByte(s, toLo(opcodeAddr));
  *(s++) = ' ';
  *(s++) = ' ';
  // Opcode bytes
  unsigned opByteCount = PC - opcodeAddr;
  for (byte_t* p = RAM + opcodeAddr; p < RAM + opcodeAddr + 3; p++) {
    if (p < RAM + PC) {
      s = putHexByte(s, *p);
    } else {
      s = spc2(s);
    }
    *(s++) = ' ';
  }
  // Instruction display
  s = spc3(s);
  s = putString(s, instructionMnemonics[inst]);
  *(s++) = ' ';
  // Operand display
  if (admdFlags & AMF_Ind) {
    *(s++) = '(';
    *(s++) = '$';
    if (admd == AM_ind)
      s = putHexByte(s, toHi(rawAddr));
    s = putHexByte(s, toLo(rawAddr));
    if (admd == AM_Xind) {
      *(s++) = ',';
      *(s++) = 'X';
    }
    *(s++) = ')';
    if (admd == AM_indY) {
      *(s++) = ',';
      *(s++) = 'Y';
    }
#ifdef TRACE_EXTRA
    s = putString(s, " -> $");
    s = putHexByte(s, toHi(operand));
    s = putHexByte(s, toLo(operand));
#endif
    assert(s <= buf + TRACE_INSTR_FLAGS_COL);
  } else if (admd == AM_rel) {
    *(s++) = '$';
    s = putHexByte(s, toHi(operand));
    s = putHexByte(s, toLo(operand));
  } else if (opByteCount == 3) {
    *(s++) = '$';
    s = putHexByte(s, toHi(rawAddr));
    s = putHexByte(s, toLo(rawAddr));
    if (admdFlags & AMF_X) {
      *(s++) = ',';
      *(s++) = 'X';
    } else if (admdFlags & AMF_Y) {
      *(s++) = ',';
      *(s++) = 'Y';
    } else {
      assert(admdFlags & AMF_NoIndex);
      *(s++) = ' ';
      *(s++) = ' ';
    }
#ifdef TRACE_EXTRA
    if (admdFlags & (AMF_X | AMF_Y)) {
      s = putString(s, "  [$");
      s = putHexByte(s, toHi(operand));
      s = putHexByte(s, toLo(operand));
      *(s++) = ']';
    }
#endif
  } else if (opByteCount == 2) {
    if (admd == AM_imm)
      *(s++) = '#';
    *(s++) = '$';
    s = putHexByte(s, operand);
  } else {
    assert(admd == AM_impl);
  }
  s = alignToColumn(s, buf, TRACE_INSTR_FLAGS_COL);
  // Registers
  s = putString(s, "- A:");
  s = putHexByte(s, A);
  s = putString(s, " X:");
  s = putHexByte(s, X);
  s = putString(s, " Y:");
  s = putHexByte(s, Y);
  s = putString(s, " SP:");
  s = putHexByte(s, m->reg.s);
  *(s++) = ' ';
  // Flags
  byte_t flags = m->reg.p;
  *(s++) = flags & FLAG_N ? 'N' : '.';
  *(s++) = flags & FLAG_V ? 'V' : '.';
  *(s++) = '-'; // unused bit 5
  *(s++) = '.'; // break flag isn't actually stored
  *(s++) = flags & FLAG_D ? 'D' : '.';
  *(s++) = flags & FLAG_I ? 'I' : '.';
  *(s++) = flags & FLAG_Z ? 'Z' : '.';
  *(s++) = flags & FLAG_C ? 'C' : '.';
  // 32-bit instruction counter
  assert(sizeof(m->reg.ic) == EXPECTED_IC_SIZE);
  s = putString(s, "  IC:");
  s = putHexByte(s, (m->reg.ic >> 24) & 0xFF);
  s = putHexByte(s, (m->reg.ic >> 16) & 0xFF);
  s = putHexByte(s, (m->reg.ic >>  8) & 0xFF);
  s = putHexByte(s, (m->reg.ic >>  0) & 0xFF);
  *s = 0;
  assert(s - buf < TRACE_BUFSIZ); // check buffer overflow
  trace(m, false, buf);
}
#endif

//|-----------------|
//| EXECUTION HOOKS |
//|-----------------|

static void expandHooksTable(emu_t* m) {
  m->hooks.cap += 16;
  m->hooks.hooks = realloc(m->hooks.hooks, m->hooks.cap * sizeof(ExecutionHook));
  m->hooks.lookup = realloc(m->hooks.lookup, m->hooks.cap * sizeof(ExecutionHooksLookupTableRow));
  if (!m->hooks.hooks) {
    fprintf(stderr, "Out of memory while expanding hooks table.\n");
    exit(1);
  }
}

void registerHook(emu_t* m, ExecutionHook* hook) {
  assert(hook != NULL);
  assert(hook->callback != NULL);
  assert(hook->pcHookAddress >= 0);
  assert(hook->pcHookAddress < RAM_SIZE);
  assert(hook->hookType >= 0);
  assert(hook->hookType < HOOKTYPE_COUNT);
  assert(hook->name != NULL);
  if (m->hooks.len == m->hooks.cap)
    expandHooksTable(m);
  m->hooks.hooks[m->hooks.len++] = *hook;
  m->hooks.ready = false;
}

int compareHooks(const void* argA, const void* argB) {
  const ExecutionHook* hookA = argA;
  const ExecutionHook* hookB = argB;
  int cmp =
    (int)hookA->pcHookAddress - (int)hookB->pcHookAddress
    || (int)hookA->hookType - (int)hookB->hookType
    || (int)hookA->isPostHook - (int)hookB->isPostHook
    || (int)hookA->hookID - (int)hookB->hookID;
  if (cmp == 0) {
    fprintf(stderr, "ERROR: Duplicate execution hook '%s'.", hookA->name);
    exit(1);
  }
  return cmp;
}

void sortHooks(emu_t* m) {
  qsort(
      m->hooks.hooks,
      m->hooks.len,
      sizeof(m->hooks.hooks[0]),
      compareHooks);
}

void buildHooksLookupTable(emu_t* m) {
  int prevHookAddress = -1;
  int prevHookType = -1;
  int hookIndex = 0;
  int lookupIndex = -1;
  for (; hookIndex < m->hooks.len; hookIndex++) {
    int pc = m->hooks.hooks[hookIndex].pcHookAddress;
    int type = m->hooks.hooks[hookIndex].hookType;
    if (pc != prevHookAddress) {
      prevHookAddress = pc;
      prevHookType = type;
      lookupIndex++;
      m->hooks.lookup[lookupIndex].pcHookAddress = pc;
      for (int i=0; i < HOOKTYPE_COUNT; i++) {
        m->hooks.lookup[lookupIndex].t[i].off = 0;
        m->hooks.lookup[lookupIndex].t[i].len = 0;
      }
      m->hooks.lookup[lookupIndex].t[type].off = hookIndex;
      m->hooks.lookup[lookupIndex].t[type].len = 1;
    } else if (type != prevHookType) {
      m->hooks.lookup[lookupIndex].t[type].off = hookIndex;
      m->hooks.lookup[lookupIndex].t[type].len = 1;
    } else {
      m->hooks.lookup[lookupIndex].t[type].len++;
    }
  }
  m->hooks.lookupLen = lookupIndex + 1;
}

void prepareHooks(emu_t* m) {
  sortHooks(m);
  buildHooksLookupTable(m);
  m->hooks.ready = true; // Mark hooks ready for lookup.
}

void lookupHooks(emu_t* m,
    int pc, int hookType, ExecutionHook** hooksStart, int* hooksCount) {
  if (!m->hooks.ready)
    error(m, "Execution hooks lookup table is not ready, prepare it first.");
  assert(hookType >= 0);
  assert(hookType < HOOKTYPE_COUNT);
  // TODO: binary search
  for (int i=0; i < m->hooks.lookupLen; i++) {
    if (m->hooks.lookup[i].pcHookAddress == pc) {
      *hooksStart = m->hooks.hooks + m->hooks.lookup[i].t[hookType].off;
      *hooksCount = m->hooks.lookup[i].t[hookType].len;
      return; // found
    }
  }
  *hooksCount = 0; // not found
}

//|-------------------------|
//| MAIN ENTRY TO EMULATION |
//|-------------------------|

void interp(emu_t* m) {
#if TRACE_ON
  prepareHooks(m);
#endif
  for (;;) {
    word_t opcodeAddr = PC;
    byte_t opcode = RAM[opcodeAddr];

    // Stop when ACS enters the FORTH interpreter.
    // For now, our goal is to reach this line.
    if (opcodeAddr == 0x0925)
      return;
    // TODO: Implement stopping point with hooks? Or some other way of
    // separating any machine-specific or game-specific functionality from the
    // emulator core.

    PC++;
    m->reg.ic++;

#if TRACE_ON
    if (m->reg.ic == INSTRUCTION_COUNT_LIMIT) {
      fprintf(stderr, "Too many instructions, stopping before the disk gets full.\n");
      return;
    }
#endif

#if TRACE_ON
    // Check for execution hooks.
    ExecutionHook* hooks; // pointer to hooks found for this PC
    int hooksCount; // will be 0 if no hooks for this PC
    lookupHooks(m, opcodeAddr, HOOKTYPE_EXEC, &hooks, &hooksCount);

    // Run pre hooks.
    for (int i=0; i < hooksCount; i++) {
      if (hooks[i].isPostHook)
        break; // Post hooks sort after pre hooks
      hooks[i].callback(m, opcodeAddr, &hooks[i]);
    }
#endif

    // DECODE INSTRUCTION

    instruction_t instr = instructionSet[opcode];
    byte_t inst = instr.instruction;
    byte_t admd = instr.addressingMode;
    if (inst == 0)
      error(m, "Illegal instruction: %02X (PC=%04X, IC=" IC_FMT ")",
          opcode, opcodeAddr, m->reg.ic);
    AddrModeFlags_t admdFlags = addrModeInfo[admd].flags;
    word_t operand = 0;
    word_t rawOperand = -1;

    //bool isIndirect = false;
    if (admdFlags & AMF_Resolve) {
      //isIndirect =
      resolveAddress(m, admd, admdFlags, &operand, &rawOperand);
    } else {
      if (admd == AM_imm) {
        operand = RAM[PC];
        PC++;
      } else {
        assert(admd == AM_impl);
      }
    }

    // TRACE

#if TRACE_ON
    traceInstruction(m, opcodeAddr, inst, admd, admdFlags, operand, rawOperand);
#endif

    // EXECUTE

    switch (admd) {
      case AM_impl:
        interpImpl(m, inst);
        break;
      case AM_imm:
        interpImm(m, inst, operand);
        break;
      default:
        interpAddr(m, inst, operand);
        break;
    }

  }
}

// Returns the address where the file was loaded.
// Sets X:Y to the end address of the loaded file (the byte after the file
// data).
word_t loadPRG(emu_t* m, buf_t* prgFile) {
  assert(m);
  assert(prgFile);
  if (prgFile->len < 2)
    error(m, "File data is too small to be a PRG file.");
  byte_t* d = prgFile->data;
  for (int i=0; i < 6; i++)
    printf("%02x ", d[i]);
  printf("\n");
  word_t loadAddr = toWord(d[0], d[1]);
  unsigned len = prgFile->len - 2;
  unsigned top = loadAddr + len;
  if (top % 0x100 != 0)
    top = (top / 0x100 + 1) * 0x100;
  printf("Loading file of length %X at $%04X\n", len, loadAddr);
  if (top >= RAM_SIZE)
    error(m, "Not enough space to load file of length %X at address %X.", len, loadAddr);
  byte_t* src = d + 2;
  byte_t* dst = m->ram + loadAddr;
  for (unsigned i = 0; i < len; i++) {
    dst[i] = src[i];
  }
  // Store the end of the loaded file in X:Y, just like the C64 LOAD routine.
  m->reg.x = toLo(top);
  m->reg.y = toHi(top);
  return loadAddr;
}

word_t expectKey(buf_t* f, unsigned off, const char* key) {
  while (*key) {
    if (off >= f->len)
      return 0;
    if (f->data[off] != *key)
      return 0;
    off++;
    key++;
  }
  if (off >= f->len)
    return 0;
  if (f->data[off] != '=')
    return 0;
  return off + 1;
}

word_t readLine(buf_t* f, unsigned off, char* s, unsigned limit) {
  unsigned start = off;
  while (off < f->len && off < limit && f->data[off] != '\n' && f->data[off] != '\r')
    off++;
  if (off == f->len || off == limit)
    return 0;
  memcpy(s, f->data + start, off - start);
  s[off - start] = 0;
  while (off < f->len && (f->data[off] != '\n' && f->data[off] != '\r'))
    off++;
  if (off == f->len)
    return 0;
  return off;
}

void loadRegisters(emu_t* m, buf_t* regFile) {
  if (regFile->len != 7)
    error(m, "Invalid register file (wrong size).");
  uint8_t* r = regFile->data;
  PC = toWord(r[0], r[1]);
  A = r[2];
  X = r[3];
  Y = r[4];
  SP = r[5];
  m->reg.p = r[6];
}

void loadRAM(emu_t* m, buf_t* ramFile) {
  if (ramFile->len != RAM_SIZE)
    error(m, "Invalid RAM file (wrong size).");
  memcpy(m->ram, ramFile->data, RAM_SIZE);
}

void mountDisk(emu_t* m, const char* path, buf_t* diskData) {
  checkDiskSize(m, diskData);
  m->diskdrive.mountedImagePath = path;
  m->diskdrive.mountedImageData = diskData;
}

void dumpRam(Emu* m, const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "Unable to open file: %s\n", path);
    exit(1);
  }
  fwrite(RAM, 1, RAM_SIZE, f);
  //for (int addr=0; addr < RAM_SIZE; addr++) {
  //  putc(RAM[addr], f);
  //}
  fclose(f);
}

emu_t* createEmulator(FILE* traceFile) {
  emu_t* m = calloc(1, sizeof(emu_t));
  if (!m) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }
  m->reg.s = 0xFF; // set S to top of stack
  m->reg.p = FLAG_B; // set B flag so BIT works as expected
  m->traceFile = traceFile;
  loadROM("rom/c64/chargen", m->rom.chargen, sizeof(m->rom.chargen));
  loadROM("rom/c64/basic", m->rom.basic, sizeof(m->rom.basic));
  loadROM("rom/c64/kernal", m->rom.kernal, sizeof(m->rom.kernal));
  return m;
}

