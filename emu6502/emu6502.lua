
local ffi = require("ffi")
local bit = require("bit")
local C = ffi.C

local function printf(...)
  print(string.format(...))
end

local MEMORY_SIZE = 0x10000

local module = {}

local byte = ffi.typeof[[
uint8_t
]]

local bytes = ffi.typeof[[
uint8_t[?]
]]

local signedByte = ffi.typeof[[
int8_t
]]

local registers = ffi.typeof[[
struct {
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint16_t s;
  uint16_t pc;
  uint8_t p;
  uint8_t ubyte; // fake register used internally
  uint32_t ic; // instruction count
}
]]

ffi.cdef[[
enum flags {
  FLAG_N = 0x80, // negative
  FLAG_V = 0x40, // overflow
  // bit 5 ignored
  FLAG_B = 0x10, // break (fake flag, only appears when flags saved in RAM)
  FLAG_D = 0x08, // decimal
  FLAG_I = 0x04, // interrupt
  FLAG_Z = 0x02, // zero
  FLAG_C = 0x01, // carry
};
enum instructions {
  ADC=1, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD,
  CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA,
  LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC, SEC,
  SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA,
};
enum addrModeFlags {
  AMF_Resolve = 0x80,
  AMF_TwoByte = 0x40,
  AMF_Ind     = 0x20,
  AMF_Rel     = 0x10,
};
enum addrModes {

  AM_impl = 0x00, // implied
  AM_imm  = 0x01, // immediate
  AM_A    = 0x00, // A is treated the same as implied

  AM_zpg  = AMF_Resolve |           0 |       0 | 0x03,
  AM_zpgX = AMF_Resolve |           0 |       0 | 0x04,
  AM_zpgY = AMF_Resolve |           0 |       0 | 0x05,

  AM_rel  = AMF_Resolve |           0 | AMF_Rel | 0x06,

  AM_abs  = AMF_Resolve | AMF_TwoByte |       0 | 0x07,
  AM_absX = AMF_Resolve | AMF_TwoByte |       0 | 0x08,
  AM_absY = AMF_Resolve | AMF_TwoByte |       0 | 0x09,

  AM_ind  = AMF_Resolve |           0 | AMF_Ind | 0x0A,
  AM_Xind = AMF_Resolve |           0 | AMF_Ind | 0x0B,
  AM_indY = AMF_Resolve |           0 | AMF_Ind | 0x0C,

};
typedef uint8_t instructionSet_t[0x100][2];
]]

local instNames = {
  [C.ADC] = "ADC",
  [C.AND] = "AND",
  [C.ASL] = "ASL",
  [C.BCC] = "BCC",
  [C.BCS] = "BCS",
  [C.BEQ] = "BEQ",
  [C.BIT] = "BIT",
  [C.BMI] = "BMI",
  [C.BNE] = "BNE",
  [C.BPL] = "BPL",
  [C.BRK] = "BRK",
  [C.BVC] = "BVC",
  [C.BVS] = "BVS",
  [C.CLC] = "CLC",
  [C.CLD] = "CLD",
  [C.CLI] = "CLI",
  [C.CLV] = "CLV",
  [C.CMP] = "CMP",
  [C.CPX] = "CPX",
  [C.CPY] = "CPY",
  [C.DEC] = "DEC",
  [C.DEX] = "DEX",
  [C.DEY] = "DEY",
  [C.EOR] = "EOR",
  [C.INC] = "INC",
  [C.INX] = "INX",
  [C.INY] = "INY",
  [C.JMP] = "JMP",
  [C.JSR] = "JSR",
  [C.LDA] = "LDA",
  [C.LDX] = "LDX",
  [C.LDY] = "LDY",
  [C.LSR] = "LSR",
  [C.NOP] = "NOP",
  [C.ORA] = "ORA",
  [C.PHA] = "PHA",
  [C.PHP] = "PHP",
  [C.PLA] = "PLA",
  [C.PLP] = "PLP",
  [C.ROL] = "ROL",
  [C.ROR] = "ROR",
  [C.RTI] = "RTI",
  [C.RTS] = "RTS",
  [C.SBC] = "SBC",
  [C.SEC] = "SEC",
  [C.SED] = "SED",
  [C.SEI] = "SEI",
  [C.STA] = "STA",
  [C.STX] = "STX",
  [C.STY] = "STY",
  [C.TAX] = "TAX",
  [C.TAY] = "TAY",
  [C.TSX] = "TSX",
  [C.TXA] = "TXA",
  [C.TXS] = "TXS",
  [C.TYA] = "TYA",
}

local addrModeNames = {
  [C.AM_impl] = "impl",
  [C.AM_imm] = "imm",

  [C.AM_zpg] = "zpg",
  [C.AM_zpgX] = "zpg,X",
  [C.AM_zpgY] = "zpg,Y",
  [C.AM_rel] = "rel",

  [C.AM_abs] = "abs",
  [C.AM_absX] = "abs,X",
  [C.AM_absY] = "abs,Y",
  [C.AM_ind] = "ind",
  [C.AM_Xind] = "X,ind",
  [C.AM_indY] = "ind,Y",
}

local instructionSet = ffi.typeof("instructionSet_t")
local decodingMap = instructionSet()

decodingMap[0x00][0] = C.BRK
decodingMap[0x00][1] = C.AM_impl
decodingMap[0x01][0] = C.ORA
decodingMap[0x01][1] = C.AM_Xind
decodingMap[0x05][0] = C.ORA
decodingMap[0x05][1] = C.AM_zpg
decodingMap[0x06][0] = C.ASL
decodingMap[0x06][1] = C.AM_zpg
decodingMap[0x08][0] = C.PHP
decodingMap[0x08][1] = C.AM_impl
decodingMap[0x09][0] = C.ORA
decodingMap[0x09][1] = C.AM_imm
decodingMap[0x0A][0] = C.ASL
decodingMap[0x0A][1] = C.AM_A
decodingMap[0x0D][0] = C.ORA
decodingMap[0x0D][1] = C.AM_abs
decodingMap[0x0E][0] = C.ASL
decodingMap[0x0E][1] = C.AM_abs

decodingMap[0x10][0] = C.BPL
decodingMap[0x10][1] = C.AM_rel
decodingMap[0x11][0] = C.ORA
decodingMap[0x11][1] = C.AM_indY
decodingMap[0x15][0] = C.ORA
decodingMap[0x15][1] = C.AM_zpg
decodingMap[0x16][0] = C.ASL
decodingMap[0x16][1] = C.AM_zpgX
decodingMap[0x18][0] = C.CLC
decodingMap[0x18][1] = C.AM_impl
decodingMap[0x19][0] = C.ORA
decodingMap[0x19][1] = C.AM_absY
decodingMap[0x1D][0] = C.ORA
decodingMap[0x1D][1] = C.AM_absX
decodingMap[0x1E][0] = C.ASL
decodingMap[0x1E][1] = C.AM_absX

decodingMap[0x20][0] = C.JSR
decodingMap[0x20][1] = C.AM_abs
decodingMap[0x21][0] = C.AND
decodingMap[0x21][1] = C.AM_Xind
decodingMap[0x24][0] = C.BIT
decodingMap[0x24][1] = C.AM_zpg
decodingMap[0x25][0] = C.AND
decodingMap[0x25][1] = C.AM_zpg
decodingMap[0x26][0] = C.ROL
decodingMap[0x26][1] = C.AM_zpg
decodingMap[0x28][0] = C.PLP
decodingMap[0x28][1] = C.AM_impl
decodingMap[0x29][0] = C.AND
decodingMap[0x29][1] = C.AM_imm
decodingMap[0x2A][0] = C.ROL
decodingMap[0x2A][1] = C.AM_A
decodingMap[0x2C][0] = C.BIT
decodingMap[0x2C][1] = C.AM_abs
decodingMap[0x2D][0] = C.AND
decodingMap[0x2D][1] = C.AM_abs
decodingMap[0x2E][0] = C.ROL
decodingMap[0x2E][1] = C.AM_abs

decodingMap[0x30][0] = C.BMI
decodingMap[0x30][1] = C.AM_rel
decodingMap[0x31][0] = C.AND
decodingMap[0x31][1] = C.AM_indY
decodingMap[0x35][0] = C.AND
decodingMap[0x35][1] = C.AM_zpg
decodingMap[0x36][0] = C.ROL
decodingMap[0x36][1] = C.AM_zpgX
decodingMap[0x38][0] = C.SEC
decodingMap[0x38][1] = C.AM_impl
decodingMap[0x39][0] = C.AND
decodingMap[0x39][1] = C.AM_absY
decodingMap[0x3D][0] = C.AND
decodingMap[0x3D][1] = C.AM_absX
decodingMap[0x3E][0] = C.ROL
decodingMap[0x3E][1] = C.AM_absX

decodingMap[0x40][0] = C.RTI
decodingMap[0x40][1] = C.AM_impl
decodingMap[0x41][0] = C.EOR
decodingMap[0x41][1] = C.AM_Xind
decodingMap[0x45][0] = C.EOR
decodingMap[0x45][1] = C.AM_zpg
decodingMap[0x46][0] = C.LSR
decodingMap[0x46][1] = C.AM_zpg
decodingMap[0x48][0] = C.PHA
decodingMap[0x48][1] = C.AM_impl
decodingMap[0x49][0] = C.EOR
decodingMap[0x49][1] = C.AM_imm
decodingMap[0x4A][0] = C.LSR
decodingMap[0x4A][1] = C.AM_A
decodingMap[0x4C][0] = C.JMP
decodingMap[0x4C][1] = C.AM_abs
decodingMap[0x4D][0] = C.EOR
decodingMap[0x4D][1] = C.AM_abs
decodingMap[0x4E][0] = C.LSR
decodingMap[0x4E][1] = C.AM_abs

decodingMap[0x50][0] = C.BVC
decodingMap[0x50][1] = C.AM_rel
decodingMap[0x51][0] = C.EOR
decodingMap[0x51][1] = C.AM_indY
decodingMap[0x55][0] = C.EOR
decodingMap[0x55][1] = C.AM_zpg
decodingMap[0x56][0] = C.LSR
decodingMap[0x56][1] = C.AM_zpg
decodingMap[0x58][0] = C.CLI
decodingMap[0x58][1] = C.AM_impl
decodingMap[0x59][0] = C.EOR
decodingMap[0x59][1] = C.AM_absY
decodingMap[0x5D][0] = C.EOR
decodingMap[0x5D][1] = C.AM_absX
decodingMap[0x5E][0] = C.LSR
decodingMap[0x5E][1] = C.AM_absX

decodingMap[0x60][0] = C.RTS
decodingMap[0x60][1] = C.AM_impl
decodingMap[0x61][0] = C.ADC
decodingMap[0x61][1] = C.AM_Xind
decodingMap[0x65][0] = C.ADC
decodingMap[0x65][1] = C.AM_zpg
decodingMap[0x66][0] = C.ROR
decodingMap[0x66][1] = C.AM_zpg
decodingMap[0x68][0] = C.PLA
decodingMap[0x68][1] = C.AM_impl
decodingMap[0x69][0] = C.ADC
decodingMap[0x69][1] = C.AM_imm
decodingMap[0x6A][0] = C.ROR
decodingMap[0x6A][1] = C.AM_A
decodingMap[0x6C][0] = C.JMP
decodingMap[0x6C][1] = C.AM_ind
decodingMap[0x6D][0] = C.ADC
decodingMap[0x6D][1] = C.AM_abs
decodingMap[0x6E][0] = C.ROR
decodingMap[0x6E][1] = C.AM_abs

decodingMap[0x70][0] = C.BVS
decodingMap[0x70][1] = C.AM_rel
decodingMap[0x71][0] = C.SBC
decodingMap[0x71][1] = C.AM_indY
decodingMap[0x75][0] = C.ADC
decodingMap[0x75][1] = C.AM_zpgX
decodingMap[0x76][0] = C.ROR
decodingMap[0x76][1] = C.AM_zpgX
decodingMap[0x78][0] = C.SEI
decodingMap[0x78][1] = C.AM_impl
decodingMap[0x79][0] = C.ADC
decodingMap[0x79][1] = C.AM_absY
decodingMap[0x7D][0] = C.ADC
decodingMap[0x7D][1] = C.AM_absY
decodingMap[0x7E][0] = C.ROR
decodingMap[0x7E][1] = C.AM_absX

decodingMap[0x81][0] = C.STA
decodingMap[0x81][1] = C.AM_Xind
decodingMap[0x84][0] = C.STY
decodingMap[0x84][1] = C.AM_zpg
decodingMap[0x85][0] = C.STA
decodingMap[0x85][1] = C.AM_zpg
decodingMap[0x86][0] = C.STX
decodingMap[0x86][1] = C.AM_impl
decodingMap[0x88][0] = C.DEY
decodingMap[0x88][1] = C.AM_impl
decodingMap[0x8A][0] = C.TXA
decodingMap[0x8A][1] = C.AM_impl
decodingMap[0x8C][0] = C.STY
decodingMap[0x8C][1] = C.AM_abs
decodingMap[0x8D][0] = C.STA
decodingMap[0x8D][1] = C.AM_abs
decodingMap[0x8E][0] = C.STX
decodingMap[0x8E][1] = C.AM_abs

decodingMap[0x90][0] = C.BCC
decodingMap[0x90][1] = C.AM_rel
decodingMap[0x91][0] = C.STA
decodingMap[0x91][1] = C.AM_indY
decodingMap[0x94][0] = C.STY
decodingMap[0x94][1] = C.AM_zpgX
decodingMap[0x95][0] = C.STA
decodingMap[0x95][1] = C.AM_zpgX
decodingMap[0x96][0] = C.STX
decodingMap[0x96][1] = C.AM_zpgY
decodingMap[0x98][0] = C.TYA
decodingMap[0x98][1] = C.AM_impl
decodingMap[0x99][0] = C.STA
decodingMap[0x99][1] = C.AM_absY
decodingMap[0x9A][0] = C.TXS
decodingMap[0x9A][1] = C.AM_impl
decodingMap[0x9D][0] = C.STA
decodingMap[0x9D][1] = C.AM_absX

decodingMap[0xA0][0] = C.LDY
decodingMap[0xA0][1] = C.AM_imm
decodingMap[0xA1][0] = C.LDA
decodingMap[0xA1][1] = C.AM_Xind
decodingMap[0xA2][0] = C.LDX
decodingMap[0xA2][1] = C.AM_imm
decodingMap[0xA4][0] = C.LDY
decodingMap[0xA4][1] = C.AM_zpg
decodingMap[0xA5][0] = C.LDA
decodingMap[0xA5][1] = C.AM_zpg
decodingMap[0xA6][0] = C.LDX
decodingMap[0xA6][1] = C.AM_zpg
decodingMap[0xA8][0] = C.TAY
decodingMap[0xA8][1] = C.AM_impl
decodingMap[0xA9][0] = C.LDA
decodingMap[0xA9][1] = C.AM_imm
decodingMap[0xAA][0] = C.TAX
decodingMap[0xAA][1] = C.AM_impl
decodingMap[0xAC][0] = C.LDY
decodingMap[0xAC][1] = C.AM_abs
decodingMap[0xAD][0] = C.LDA
decodingMap[0xAD][1] = C.AM_abs
decodingMap[0xAE][0] = C.LDX
decodingMap[0xAE][1] = C.AM_abs

decodingMap[0xB0][0] = C.BCS
decodingMap[0xB0][1] = C.AM_rel
decodingMap[0xB1][0] = C.LDA
decodingMap[0xB1][1] = C.AM_indY
decodingMap[0xB4][0] = C.LDY
decodingMap[0xB4][1] = C.AM_zpgX
decodingMap[0xB5][0] = C.LDA
decodingMap[0xB5][1] = C.AM_zpgX
decodingMap[0xB6][0] = C.LDX
decodingMap[0xB6][1] = C.AM_zpgY
decodingMap[0xB8][0] = C.CLV
decodingMap[0xB8][1] = C.AM_impl
decodingMap[0xB9][0] = C.LDA
decodingMap[0xB9][1] = C.AM_absY
decodingMap[0xBA][0] = C.TSX
decodingMap[0xBA][1] = C.AM_impl
decodingMap[0xBC][0] = C.LDY
decodingMap[0xBC][1] = C.AM_absX
decodingMap[0xBD][0] = C.LDA
decodingMap[0xBD][1] = C.AM_absX
decodingMap[0xBE][0] = C.LDX
decodingMap[0xBE][1] = C.AM_absY

decodingMap[0xC0][0] = C.CPY
decodingMap[0xC0][1] = C.AM_imm
decodingMap[0xC1][0] = C.CMP
decodingMap[0xC1][1] = C.AM_Xind
decodingMap[0xC4][0] = C.CPY
decodingMap[0xC4][1] = C.AM_zpg
decodingMap[0xC5][0] = C.CMP
decodingMap[0xC5][1] = C.AM_zpg
decodingMap[0xC6][0] = C.DEC
decodingMap[0xC6][1] = C.AM_zpg
decodingMap[0xC8][0] = C.INY
decodingMap[0xC8][1] = C.AM_impl
decodingMap[0xC9][0] = C.CMP
decodingMap[0xC9][1] = C.AM_imm
decodingMap[0xCA][0] = C.DEX
decodingMap[0xCA][1] = C.AM_impl
decodingMap[0xCC][0] = C.CPY
decodingMap[0xCC][1] = C.AM_abs
decodingMap[0xCD][0] = C.CMP
decodingMap[0xCD][1] = C.AM_abs
decodingMap[0xCE][0] = C.DEC
decodingMap[0xCE][1] = C.AM_abs

decodingMap[0xD0][0] = C.BNE
decodingMap[0xD0][1] = C.AM_rel
decodingMap[0xD1][0] = C.CMP
decodingMap[0xD1][1] = C.AM_indY
decodingMap[0xD5][0] = C.CMP
decodingMap[0xD5][1] = C.AM_zpgX
decodingMap[0xD6][0] = C.DEC
decodingMap[0xD6][1] = C.AM_zpgX
decodingMap[0xD8][0] = C.CLD
decodingMap[0xD8][1] = C.AM_impl
decodingMap[0xD9][0] = C.CMP
decodingMap[0xD9][1] = C.AM_absY
decodingMap[0xDD][0] = C.CMP
decodingMap[0xDD][1] = C.AM_absX
decodingMap[0xDE][0] = C.DEC
decodingMap[0xDE][1] = C.AM_absX

decodingMap[0xE0][0] = C.CPX
decodingMap[0xE0][1] = C.AM_imm
decodingMap[0xE1][0] = C.SBC
decodingMap[0xE1][1] = C.AM_Xind
decodingMap[0xE4][0] = C.CPX
decodingMap[0xE4][1] = C.AM_zpg
decodingMap[0xE5][0] = C.SBC
decodingMap[0xE5][1] = C.AM_zpg
decodingMap[0xE6][0] = C.INC
decodingMap[0xE6][1] = C.AM_zpg
decodingMap[0xE8][0] = C.INX
decodingMap[0xE8][1] = C.AM_impl
decodingMap[0xE9][0] = C.SBC
decodingMap[0xE9][1] = C.AM_imm
decodingMap[0xEA][0] = C.NOP
decodingMap[0xEA][1] = C.AM_impl
decodingMap[0xEC][0] = C.CPX
decodingMap[0xEC][1] = C.AM_abs
decodingMap[0xED][0] = C.SBC
decodingMap[0xED][1] = C.AM_abs
decodingMap[0xEE][0] = C.INC
decodingMap[0xEE][1] = C.AM_abs

decodingMap[0xF0][0] = C.BEQ
decodingMap[0xF0][1] = C.AM_rel
decodingMap[0xF1][0] = C.SBC
decodingMap[0xF1][1] = C.AM_indY
decodingMap[0xF5][0] = C.SBC
decodingMap[0xF5][1] = C.AM_zpgX
decodingMap[0xF6][0] = C.INC
decodingMap[0xF6][1] = C.AM_zpgX
decodingMap[0xF8][0] = C.SED
decodingMap[0xF8][1] = C.AM_impl
decodingMap[0xF9][0] = C.SBC
decodingMap[0xF9][1] = C.AM_absY
decodingMap[0xFD][0] = C.SBC
decodingMap[0xFD][1] = C.AM_absX
decodingMap[0xFE][0] = C.INC
decodingMap[0xFE][1] = C.AM_absX

function module._expectMode(expected, admd)
  if not (admd == expected) then
    local msg = string.format(
    "Expected address mode '%s' here, got '%s' (%02X) instead.",
    addrModeNames[expected], addrModeNames[admd], admd)
    error(msg, 2)
  end
end

local function deref(m, pointer)
  local addr = bit.bor(m.ram[pointer], bit.lshift(m.ram[pointer+1], 8))
  printf(" Dereferencing pointer: %04X -> %04X", pointer, addr)
  return addr
end

function module._resolveAddress(m, admd)
  addr = m.ram[m.reg.pc]
  m.reg.pc = m.reg.pc + 1
  if bit.band(admd, C.AMF_TwoByte) ~= 0 then
    addr = bit.bor(addr, bit.lshift(m.ram[m.reg.pc], 8))
    m.reg.pc = m.reg.pc + 1
    -- absolute
    if admd == C.AM_absX then
      addr = addr + m.reg.x
    elseif admd == C.AM_absY then
      addr = addr + m.reg.y
    else
      module._expectMode(C.AM_abs, admd)
    end
  else
    if bit.band(admd, C.AMF_Ind) ~= 0 then
      -- indirect
      if admd == C.AM_Xind then
        addr = deref(m, addr + m.reg.x)
      else
        addr = deref(m, addr)
        if admd == C.AM_indY then
          addr = addr + m.reg.y
        else
          module._expectMode(C.AM_ind, admd)
        end
      end
    elseif bit.band(admd, C.AMF_Rel) ~= 0 then
      -- relative (branch)
      module._expectMode(C.AM_rel, admd)
      addr = m.reg.pc + signedByte(addr)
    else
      -- zero page
      if admd == C.AM_zpgX then
        addr = addr + m.reg.x
      elseif admd == C.AM_zpgY then
        addr = addr + m.reg.y
      else
        assert(admd == C.AM_zpg)
      end
    end
  end
  return addr
end

local function getFlag(m, flag)
  return bit.band(m.reg.p, flag) ~= 0
end

local function setFlag(m, flag, set)
  tonumber(flag)
  assert(type(set) == "boolean")
  if set then
    m.reg.p = bit.bor(m.reg.p, flag)
  else
    m.reg.p = bit.band(m.reg.p, bit.bnot(flag))
  end
end

-- Set the Negative and Zero flags according to this byte.
local function setNZ(m, byteValue)
  if byteValue == 0 then
    setFlag(m, C.FLAG_Z, true)
    setFlag(m, C.FLAG_N, false)
  else
    setFlag(m, C.FLAG_Z, false)
    setFlag(m, C.FLAG_N,
      bit.band(byteValue, 0x80) ~= 0)
  end
end

local function stackToString(m, elt, sep)
  local s = string.format("%02x %s", elt, sep)
  for a = m.reg.s+1, 0xFF do
    s = s .. string.format(" %02x", m.ram[a])
  end
  return s
end

local function printStack(m, elt, sep)
  local s = stackToString(m, elt, sep)
  print(string.format("%21sSTACK: %s", "", s))
end

local function push(m, operand)
  assert(m.reg.s > 0, "Stack overflow.")
  printStack(m, operand, ">>")
  m.ram[m.reg.s] = operand
  m.reg.s = m.reg.s - 1
end

local function pull(m)
  assert(m.reg.s < 0xFF, "Stack underflow.")
  m.reg.s = m.reg.s + 1
  local v = m.ram[m.reg.s]
  setNZ(m, v)
  printStack(m, v, "<<")
  return v
end

-- This performs all the logic for ADC, SBC and CMP, except that SBC and CMP
-- should XOR the operand first.
local function add(m, reg, operand, isCmp)
  local diff = reg + operand
  if isCmp or getFlag(m, C.FLAG_C) then
    diff = diff + 1
  end
  m.reg.ubyte = diff
  setNZ(m, m.reg.ubyte)
  setFlag(m, C.FLAG_C, bit.band(diff, 0x100) ~= 0)
  if not isCmp then
    -- overflow = (M^result)&(N^result)&0x80
    local o1 = bit.bxor(reg, m.reg.ubyte)
    local o2 = bit.bxor(operand, m.reg.ubyte)
    local overflow = bit.band(bit.band(o1, o2), 0x80) ~= 0
    setFlag(m, C.FLAG_V, overflow)
    m.reg.a = m.reg.ubyte
  end
end

local function returnFromSub(m)
  local returnAddr = pull(m)
  returnAddr = bit.bor(returnAddr, bit.lshift(pull(m), 8))
  returnAddr = returnAddr + 1 -- correct for how JSR pushes address
  printf("%21sPC: %04X -> %04X", "", tonumber(m.reg.pc), tonumber(returnAddr))
  m.reg.pc = returnAddr
end

local function jump(m, addr, far)
  printf("%21sPC: %04X -> %04X", "", tonumber(m.reg.pc), tonumber(addr))
  if far and addr >= 0xF000 then
    local proc
    if addr == 0xFFC3 then
      proc = "CLOSE"
      args = { {"A", m.reg.a} }
    else
      error(string.format("Unsupported ROM procedure: %04X", addr))
    end
    local argStr = ""
    for i, a in ipairs(args) do
      if i > 1 then argStr = argStr .. ", " end
      argStr = argStr .. string.format("%s=%02X", unpack(a))
    end
    if string.len(argStr) > 0 then
      argStr = "(" .. argStr .. ")"
    end
    print("%21sROM CALL: %04X %s%s", "", proc, argStr)
    returnFromSub(m)
  else
    m.reg.pc = addr
  end
end

function module._interpAddr(m, inst, addr)
  assert(m)
  assert(inst)
  assert(addr, "Address is nil.")

  if inst == C.JSR then
    local pushAddr = m.reg.pc - 1
    -- push high byte first
    push(m, bit.rshift(pushAddr, 8))
    push(m, bit.band(pushAddr, 0xFF))
    jump(m, addr, true)
  elseif inst == C.JMP then
    jump(m, addr, true)

  -- Branch

  elseif inst == C.BPL then
    if not getFlag(m, C.FLAG_N) then
      jump(m, addr)
    end
  elseif inst == C.BMI then
    if getFlag(m, C.FLAG_N) then
      jump(m, addr)
    end
  elseif inst == C.BVS then
    if getFlag(m, C.FLAG_V) then
      jump(m, addr)
    end
  elseif inst == C.BCC then
    if not getFlag(m, C.FLAG_C) then
      jump(m, addr)
    end
  elseif inst == C.BCS then
    if getFlag(m, C.FLAG_C) then
      jump(m, addr)
    end
  elseif inst == C.BNE then
    if not getFlag(m, C.FLAG_Z) then
      jump(m, addr)
    end
  elseif inst == C.BEQ then
    if getFlag(m, C.FLAG_Z) then
      jump(m, addr)
    end

  -- Store

  elseif inst == C.STA then
    print(string.format("%21s%04X: %02x -> %02x", "", addr, m.ram[addr], m.reg.a))
    m.ram[addr] = m.reg.a
  elseif inst == C.STX then
    m.ram[addr] = m.reg.x
  elseif inst == C.STY then
    m.ram[addr] = m.reg.y
  elseif inst == C.BIT then
    m.ram[addr] = m.reg.p

  -- Increment/decrement

  elseif inst == C.INC then
    m.reg.ubyte = m.ram[addr] + 1
    print(string.format("%21s%04X: %02x -> %02x", "", addr, m.ram[addr], m.reg.ubyte))
    m.ram[addr] = m.reg.ubyte
    setNZ(m, m.reg.ubyte)
  elseif inst == C.DEC then
    m.reg.ubyte = m.ram[addr] - 1
    print(string.format("%21s%04X: %02x -> %02x", "", addr, m.ram[addr], m.reg.ubyte))
    m.ram[addr] = m.reg.ubyte
    setNZ(m, m.reg.ubyte)

  -- Bit shift instructions operating on memory
  elseif inst == C.ASL then
    m.reg.ubyte = m.ram[addr]
    setFlag(FLAG_C, bit.band(m.reg.ubyte, 0x80) ~= 0)
    m.reg.ubyte = bit.lshift(m.reg.ubyte, 1)
    setNZ(m, m.reg.ubyte)
    m.ram[addr] = m.reg.ubyte
  elseif inst == C.ROL then
    m.reg.ubyte = m.ram[addr]
    local carrySet = getFlag(m, C.FLAG_C)
    setFlag(m, C.FLAG_C, bit.band(m.reg.ubyte, 0x80) ~= 0)
    m.reg.ubyte = bit.lshift(m.reg.ubyte, 1)
    if carrySet then
      m.reg.ubyte = m.reg.ubyte + 1
    end
    setNZ(m, m.reg.ubyte)
    m.ram[addr] = m.reg.ubyte
  elseif inst == C.LSR then
    m.reg.ubyte = m.ram[addr]
    setFlag(m, C.FLAG_C, bit.band(m.reg.ubyte, 1) ~= 0)
    m.reg.ubyte = bit.rshift(m.reg.ubyte, 1)
    setNZ(m, m.reg.ubyte)
    m.ram[addr] = m.reg.ubyte
  elseif inst == C.ROR then
    m.reg.ubyte = m.ram[addr]
    local carrySet = getFlag(m, C.FLAG_C)
    setFlag(m, C.FLAG_C, bit.band(m.reg.ubyte, 1) ~= 0)
    m.reg.ubyte = bit.rshift(m.reg.ubyte, 1)
    if carrySet then
      m.reg.ubyte = bit.bor(m.reg.ubyte, 0x80)
    end
    setNZ(m, m.reg.ubyte)
    m.ram[addr] = m.reg.ubyte

  else
    -- The opcodes with immediate arguments can be applied to memory just by
    -- loading the value from memory and calling the same code.
    module._interpImm(m, inst, m.ram[addr])

  end
end

function module._interpImm(m, inst, operand)

  if inst == C.LDA then
    m.reg.a = operand
    setNZ(m, m.reg.a)
  elseif inst == C.LDX then
    m.reg.x = operand
    setNZ(m, m.reg.x)
  elseif inst == C.LDY then
    m.reg.y = operand
    setNZ(m, m.reg.y)

  elseif inst == C.ADC then
    add(m, m.reg.a, operand)
  elseif inst == C.SBC then
    add(m, m.reg.a, bit.bnot(operand))
  elseif inst == C.CMP then
    add(m, m.reg.a, bit.bnot(operand), true)
  elseif inst == C.CPY then
    add(m, m.reg.y, bit.bnot(operand), true)
  elseif inst == C.CPX then
    add(m, m.reg.x, bit.bnot(operand), true)

  elseif inst == C.ORA then
    m.reg.a = bit.bor(m.reg.a, operand)
    setNZ(m, m.reg.a)
  elseif inst == C.AND then
    m.reg.a = bit.band(m.reg.a, operand)
    setNZ(m, m.reg.a)
  elseif inst == C.EOR then
    m.reg.a = bit.bxor(m.reg.a, operand)
    setNZ(m, m.reg.a)

  end
end

function module._interpImpl(m, inst)
  local operand

  if inst == C.RTS then
    assert(m.reg.s <= 0xFD, "Stack underflow in RTS.")
    returnFromSub(m)

  -- Stack

  elseif inst == C.PHP then
    push(m, m.reg.p)
  elseif inst == C.PLP then
    m.reg.p = pull(m)
  elseif inst == C.PHA then
    push(m, m.reg.a)
  elseif inst == C.PLA then
    m.reg.a = pull(m)

  -- Flags

  elseif inst == C.CLC then
    setFlag(m, C.FLAG_C, false)
  elseif inst == C.SEC then
    setFlag(m, C.FLAG_C, true)
  elseif inst == C.CLV then
    setFlag(m, C.FLAG_V, false)
  elseif inst == C.CLD then
    setFlag(m, C.FLAG_D, false)
  elseif inst == C.SED then
    setFlag(m, C.FLAG_D, true)

  -- Increment/decrement
  elseif inst == C.INX then
    m.reg.x = m.reg.x + 1
    setNZ(m, m.reg.x)
  elseif inst == C.DEX then
    m.reg.x = m.reg.x - 1
    setNZ(m, m.reg.x)
  elseif inst == C.INY then
    m.reg.y = m.reg.y + 1
    setNZ(m, m.reg.y)
  elseif inst == C.DEY then
    m.reg.y = m.reg.y - 1
    setNZ(m, m.reg.y)

  -- Transfer registers
  elseif inst == C.TYA then
    m.reg.a = m.reg.y
    setNZ(m, m.reg.a)
    --if m.reg.a == 0 then print("TYA: ZERO") end
  elseif inst == C.TAY then
    m.reg.y = m.reg.a
    setNZ(m, m.reg.y)
  elseif inst == C.TXA then
    m.reg.a = m.reg.x
    setNZ(m, m.reg.a)
  elseif inst == C.TXS then
    m.reg.s = m.reg.x
    -- This instruction doesn't set N/Z
  elseif inst == C.TAX then
    m.reg.x = m.reg.a
    setNZ(m, m.reg.x)
  elseif inst == C.TSX then
    m.reg.x = m.reg.s
    setNZ(m, m.reg.x)

  elseif inst == C.NOP then
    -- do nothing

  -- Bit shift instructions operating on A

  elseif inst == C.ASL then
    setFlag(m, C.FLAG_C, bit.band(m.reg.a, 0x80) ~= 0)
    m.reg.a = bit.lshift(m.reg.a, 1)
    setNZ(m, m.reg.a)
  elseif inst == C.ROL then
    local carrySet = getFlag(m, C.FLAG_C)
    setFlag(m, C.FLAG_C, bit.band(m.reg.a, 0x80) ~= 0)
    m.reg.a = bit.lshift(m.reg.a, 1)
    if carrySet then
      m.reg.a = m.reg.a + 1
    end
    setNZ(m, m.reg.a)
  elseif inst == C.LSR then
    setFlag(m, C.FLAG_C, bit.band(m.reg.a, 1) ~= 0)
    m.reg.a = bit.rshift(m.reg.a, 1)
    setNZ(m, m.reg.a)
  elseif inst == C.ROR then
    local carrySet = getFlag(m, C.FLAG_C)
    setFlag(m, C.FLAG_C, bit.band(m.reg.a, 1) ~= 0)
    m.reg.a = bit.rshift(m.reg.a, 1)
    if carrySet then
      m.reg.a = bit.bor(m.reg.a, 0x80)
    end
    setNZ(m, m.reg.a)

  -- Instructions that are not supported because we're not emulating
  -- interrupts.

  elseif inst == C.BRK then
    error("BRK not supported.")
  elseif inst == C.RTI then
    error("RTI not supported.")
  elseif inst == C.CLI then
    error("CLI: Interrupt flag not supported.")
  elseif inst == C.SEI then
    error("SEI: Interrupt flag not supported.")

  else
    error(string.format("Unexpected instruction: %02X %s (PC=%04X, IC=%d)", inst, instNames[inst], m.reg.pc, m.reg.ic))
  end
end

function module._interp(machine)
  local m = machine
  local C = ffi.C
  local band, bor, lshift = bit.band, bit.bor, bit.lshift
  local interpSub
  while true do
    local instrPC = m.reg.pc
    local opcode = m.ram[m.reg.pc]
    m.reg.pc = m.reg.pc + 1
    m.reg.ic = m.reg.ic + 1
    -- DECODE
    local inst = decodingMap[opcode][0];
    if inst == 0 then
      error(string.format("Illegal opcode: $%02X (PC=%04X, IC=%d)", opcode, instrPC, m.reg.ic))
    end
    local admd = decodingMap[opcode][1];
    assert(instNames[inst], string.format("Missing name for instruction: %d / $%02X", inst, inst))
    assert(addrModeNames[admd])
    local operand = 0
    if band(admd, C.AMF_Resolve) ~= 0 then
      operand = module._resolveAddress(m, admd)
      interpSub = module._interpAddr
    else
      if admd == C.AM_imm then
        operand = m.ram[m.reg.pc]
        m.reg.pc = m.reg.pc + 1
        interpSub = module._interpImm
      else
        interpSub = module._interpImpl
      end
    end
    -- TRACE
    local instrDisplay = ""
    local opByteCount = 0
    for i = 0, 2 do
      if instrPC + i < m.reg.pc then
        instrDisplay = instrDisplay .. string.format("%02X ", m.ram[instrPC + i])
        opByteCount = opByteCount + 1
      else
        instrDisplay = instrDisplay .. "   "
      end
    end
    local operandDisplay = ""
    if opByteCount == 3 or admd == C.AM_rel then
      operandDisplay = string.format("$%04X", tonumber(operand))
    elseif opByteCount == 2 then
      operandDisplay = string.format("$%02X", tonumber(operand))
    end
    if opByteCount >= 2 and interpSub == module._interpImm then
      operandDisplay = "#" .. operandDisplay
    end
    local flags = ""
    if getFlag(m, C.FLAG_N) then flags = flags.."N" else flags = flags.."." end
    if getFlag(m, C.FLAG_V) then flags = flags.."V" else flags = flags.."." end
    flags = flags.."-"
    flags = flags.."."
    if getFlag(m, C.FLAG_D) then flags = flags.."D" else flags = flags.."." end
    if getFlag(m, C.FLAG_I) then flags = flags.."I" else flags = flags.."." end
    if getFlag(m, C.FLAG_Z) then flags = flags.."Z" else flags = flags.."." end
    if getFlag(m, C.FLAG_C) then flags = flags.."C" else flags = flags.."." end
    local registers = string.format(
    "- A:%02x X:%02x Y:%02x SP:%02x %s", m.reg.a, m.reg.x, m.reg.y, m.reg.s, flags)
    print(string.format(".C:%04X  %s   %s %5s %6s  %s  IC=%d", instrPC, instrDisplay, instNames[inst],
    addrModeNames[admd],
    operandDisplay,
    registers, m.reg.ic))
    -- EXECUTE
    interpSub(m, inst, operand)
  end
end

function module._loadPRG(self, prgFileData)
  assert(self)
  assert(prgFileData)
  local lo, hi = prgFileData[1], prgFileData[2]
  local addr = lo + bit.lshift(hi, 8)
  local len = #prgFileData - 2
  local top = addr + len
  if top >= MEMORY_SIZE then
    error(string.format("Not enough space to load file of length %X at address %X.", len, addr))
  end
  local base = addr - 3
  for i = 3, #prgFileData do
    self.ram[base + i] = prgFileData[i]
  end
  -- Store the end of the loaded file in X:Y, just like the C64 LOAD routine.
  self.reg.x = bit.band(top, 0xFF)
  self.reg.y = bit.rshift(top, 8)
  return addr
end

function module._createEmulator()
  local reg = registers()
  reg.s = 0xFF
  reg.p = C.FLAG_B -- set the break flag to make PHP work as expected
  return {
    ram = bytes(MEMORY_SIZE),
    reg = reg,
    word = function(self, addr)
      return bit.bor(self.ram[addr], bit.lshift(self.ram[addr+1], 8))
    end,
    loadPRG = module._loadPRG,
    setPC = function(self, addr)
      assert(addr and addr >= 0 and addr < MEMORY_SIZE, "Bad PC value.")
      self.reg.pc = addr
    end,
    advPC = function(self)
      self.reg.pc = self.reg.pc + 1
      print("PC: " .. tonumber(self.reg.pc))
    end,
    interpret = assert(module._interp),
  }
end

module.createEmulator = module._createEmulator

return module

