
-- Define the instruction set for the 6502.

-- Use Lua to do this because it makes it easy to define a sparse array (not
-- all opcodes are defined) and then generate a C array initializer from that.
-- Luajit allows us to parse the header file and use the symbolic references
-- from ""

local printf = function(...) print(string.format(...)) end

local ins = {}

ins[0x00] = { "BRK", "impl" }
ins[0x01] = { "ORA", "Xind" }
ins[0x05] = { "ORA", "zpg" }
ins[0x06] = { "ASL", "zpg" }
ins[0x08] = { "PHP", "impl" }
ins[0x09] = { "ORA", "imm" }
ins[0x0A] = { "ASL", "A" }
ins[0x0D] = { "ORA", "abs" }
ins[0x0E] = { "ASL", "abs" }

ins[0x10] = { "BPL", "rel" }
ins[0x11] = { "ORA", "indY" }
ins[0x15] = { "ORA", "zpg" }
ins[0x16] = { "ASL", "zpgX" }
ins[0x18] = { "CLC", "impl" }
ins[0x19] = { "ORA", "absY" }
ins[0x1D] = { "ORA", "absX" }
ins[0x1E] = { "ASL", "absX" }

ins[0x20] = { "JSR", "abs" }
ins[0x21] = { "AND", "Xind" }
ins[0x24] = { "BIT", "zpg" }
ins[0x25] = { "AND", "zpg" }
ins[0x26] = { "ROL", "zpg" }
ins[0x28] = { "PLP", "impl" }
ins[0x29] = { "AND", "imm" }
ins[0x2A] = { "ROL", "A" }
ins[0x2C] = { "BIT", "abs" }
ins[0x2D] = { "AND", "abs" }
ins[0x2E] = { "ROL", "abs" }

ins[0x30] = { "BMI", "rel" }
ins[0x31] = { "AND", "indY" }
ins[0x35] = { "AND", "zpg" }
ins[0x36] = { "ROL", "zpgX" }
ins[0x38] = { "SEC", "impl" }
ins[0x39] = { "AND", "absY" }
ins[0x3D] = { "AND", "absX" }
ins[0x3E] = { "ROL", "absX" }

ins[0x40] = { "RTI", "impl" }
ins[0x41] = { "EOR", "Xind" }
ins[0x45] = { "EOR", "zpg" }
ins[0x46] = { "LSR", "zpg" }
ins[0x48] = { "PHA", "impl" }
ins[0x49] = { "EOR", "imm" }
ins[0x4A] = { "LSR", "A" }
ins[0x4C] = { "JMP", "abs" }
ins[0x4D] = { "EOR", "abs" }
ins[0x4E] = { "LSR", "abs" }

ins[0x50] = { "BVC", "rel" }
ins[0x51] = { "EOR", "indY" }
ins[0x55] = { "EOR", "zpg" }
ins[0x56] = { "LSR", "zpg" }
ins[0x58] = { "CLI", "impl" }
ins[0x59] = { "EOR", "absY" }
ins[0x5D] = { "EOR", "absX" }
ins[0x5E] = { "LSR", "absX" }

ins[0x60] = { "RTS", "impl" }
ins[0x61] = { "ADC", "Xind" }
ins[0x65] = { "ADC", "zpg" }
ins[0x66] = { "ROR", "zpg" }
ins[0x68] = { "PLA", "impl" }
ins[0x69] = { "ADC", "imm" }
ins[0x6A] = { "ROR", "A" }
ins[0x6C] = { "JMP", "ind" }
ins[0x6D] = { "ADC", "abs" }
ins[0x6E] = { "ROR", "abs" }

ins[0x70] = { "BVS", "rel" }
ins[0x71] = { "SBC", "indY" }
ins[0x75] = { "ADC", "zpgX" }
ins[0x76] = { "ROR", "zpgX" }
ins[0x78] = { "SEI", "impl" }
ins[0x79] = { "ADC", "absY" }
ins[0x7D] = { "ADC", "absY" }
ins[0x7E] = { "ROR", "absX" }

ins[0x81] = { "STA", "Xind" }
ins[0x84] = { "STY", "zpg" }
ins[0x85] = { "STA", "zpg" }
ins[0x86] = { "STX", "zpg" }
ins[0x88] = { "DEY", "impl" }
ins[0x8A] = { "TXA", "impl" }
ins[0x8C] = { "STY", "abs" }
ins[0x8D] = { "STA", "abs" }
ins[0x8E] = { "STX", "abs" }

ins[0x90] = { "BCC", "rel" }
ins[0x91] = { "STA", "indY" }
ins[0x94] = { "STY", "zpgX" }
ins[0x95] = { "STA", "zpgX" }
ins[0x96] = { "STX", "zpgY" }
ins[0x98] = { "TYA", "impl" }
ins[0x99] = { "STA", "absY" }
ins[0x9A] = { "TXS", "impl" }
ins[0x9D] = { "STA", "absX" }

ins[0xA0] = { "LDY", "imm" }
ins[0xA1] = { "LDA", "Xind" }
ins[0xA2] = { "LDX", "imm" }
ins[0xA4] = { "LDY", "zpg" }
ins[0xA5] = { "LDA", "zpg" }
ins[0xA6] = { "LDX", "zpg" }
ins[0xA8] = { "TAY", "impl" }
ins[0xA9] = { "LDA", "imm" }
ins[0xAA] = { "TAX", "impl" }
ins[0xAC] = { "LDY", "abs" }
ins[0xAD] = { "LDA", "abs" }
ins[0xAE] = { "LDX", "abs" }

ins[0xB0] = { "BCS", "rel" }
ins[0xB1] = { "LDA", "indY" }
ins[0xB4] = { "LDY", "zpgX" }
ins[0xB5] = { "LDA", "zpgX" }
ins[0xB6] = { "LDX", "zpgY" }
ins[0xB8] = { "CLV", "impl" }
ins[0xB9] = { "LDA", "absY" }
ins[0xBA] = { "TSX", "impl" }
ins[0xBC] = { "LDY", "absX" }
ins[0xBD] = { "LDA", "absX" }
ins[0xBE] = { "LDX", "absY" }

ins[0xC0] = { "CPY", "imm" }
ins[0xC1] = { "CMP", "Xind" }
ins[0xC4] = { "CPY", "zpg" }
ins[0xC5] = { "CMP", "zpg" }
ins[0xC6] = { "DEC", "zpg" }
ins[0xC8] = { "INY", "impl" }
ins[0xC9] = { "CMP", "imm" }
ins[0xCA] = { "DEX", "impl" }
ins[0xCC] = { "CPY", "abs" }
ins[0xCD] = { "CMP", "abs" }
ins[0xCE] = { "DEC", "abs" }

ins[0xD0] = { "BNE", "rel" }
ins[0xD1] = { "CMP", "indY" }
ins[0xD5] = { "CMP", "zpgX" }
ins[0xD6] = { "DEC", "zpgX" }
ins[0xD8] = { "CLD", "impl" }
ins[0xD9] = { "CMP", "absY" }
ins[0xDD] = { "CMP", "absX" }
ins[0xDE] = { "DEC", "absX" }

ins[0xE0] = { "CPX", "imm" }
ins[0xE1] = { "SBC", "Xind" }
ins[0xE4] = { "CPX", "zpg" }
ins[0xE5] = { "SBC", "zpg" }
ins[0xE6] = { "INC", "zpg" }
ins[0xE8] = { "INX", "impl" }
ins[0xE9] = { "SBC", "imm" }
ins[0xEA] = { "NOP", "impl" }
ins[0xEC] = { "CPX", "abs" }
ins[0xED] = { "SBC", "abs" }
ins[0xEE] = { "INC", "abs" }

ins[0xF0] = { "BEQ", "rel" }
ins[0xF1] = { "SBC", "indY" }
ins[0xF5] = { "SBC", "zpgX" }
ins[0xF6] = { "INC", "zpgX" }
ins[0xF8] = { "SED", "impl" }
ins[0xF9] = { "SBC", "absY" }
ins[0xFD] = { "SBC", "absX" }
ins[0xFE] = { "INC", "absX" }

for opcode = 0, 0xFF do
  local details = ins[opcode] or { "0", "0" }
  local i, a = unpack(details);
  if a ~= "0" then a = "AM_"..a end
  printf("  { %s, %s },", i, a)
end

