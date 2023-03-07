
#[macro_use]
extern crate bitflags;

const RAM_SIZE: usize = 0x10000;

fn word(lo: u8, hi: u8) -> u16 {
    (lo as u16) & ((hi as u16) << 8)
}

fn word_hi(w: u16) -> u8 {
    w >> 8
}

fn word_lo(w: u16) -> u8 {
    w & 0x00FF
}

//#[derive(Debug)]
struct Mem {
    ram: [u8; RAM_SIZE],
}

impl Mem {
    // TODO: Handle special regions.
    pub fn read(&self, addr: u16) -> u8 {
        self.ram[addr as usize]
    }
    pub fn write(&mut self, addr: u16, val: u8) {
        self.ram[addr as usize] = val;
    }
    pub fn read_word(&self, addr: u16) -> u16 {
        word(self.read(addr), self.read(addr + 1))
    }
}


struct Flag(u8);

pub mod Flag {
    pub const N: Flag = Flag(0x80); // negative
    pub const V: Flag = Flag(0x40); // overflow
    // bit 5 ignored
    pub const B: Flag = Flag(0x10); // break (fake flag, only appears when flags saved in RAM)
    pub const D: Flag = Flag(0x08); // decimal
    pub const I: Flag = Flag(0x04); // interrupt
    pub const Z: Flag = Flag(0x02); // zero
    pub const C: Flag = Flag(0x01); // carry
}

struct Registers {
    a: u8,
    x: u8,
    y: u8,
    p: u8, // flags register
    s: u16, // stack pointer
    pc: u16, // program counter
    ic: usize, // instruction count
}

#[derive(Clone,Copy)]
enum Opcode {
    ADC=1, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK,
    BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX,
    DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR,
    NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC,
    SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS,
    TYA,
}

pub mod AddrModeFlag {
    pub const Resolve : u8 = 1 << 0;
    //pub const TwoByte : u8 = 1 << 1;
    pub const Ind     : u8 = 1 << 2;
    pub const Abs     : u8 = 1 << 3;
    pub const Zpg     : u8 = 1 << 4;
    pub const X       : u8 = 1 << 5;
    pub const Y       : u8 = 1 << 6;
    pub const NoIndex : u8 = 1 << 7;
}

#[derive(Clone,Copy)]
enum AddrMode {

  // treat A as implied

  Impl = 0x00, // implied
  Imm  = 0x01, // immediate

  Zpg  = 0x03,
  ZpgX = 0x04,
  ZpgY = 0x05,

  Rel  = 0x06,

  Abs  = 0x07,
  AbsX = 0x08,
  AbsY = 0x09,

  Ind  = 0x0A,
  XInd = 0x0B,
  IndY = 0x0C,

}

struct Cpu {
    reg: Registers,
}

impl Cpu {
    pub fn set_flag(&mut self, flag: Flag, set: bool) {
        if set {
            self.reg.p |= flag as u8;
        } else {
            self.reg.p &= !(flag as u8);
        }
    }
    pub fn get_flag(&self, flag: Flag) -> bool {
        0 != (self.reg.p & (flag as u8))
    }
}

struct Computer {
    cpu: Cpu,
    mem: Mem,
}

fn main() {
    println!("Hello, world!");
}

