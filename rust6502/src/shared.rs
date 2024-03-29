
use num_derive::FromPrimitive;
//use num_traits::FromPrimitive;
use std::convert::TryInto;

pub const C64_RAM_SIZE: usize = 64 * 1024;
pub const C1541_RAM_SIZE: usize = 2 * 1024;

pub fn word(lo: u8, hi: u8) -> u16 {
    (lo as u16) & ((hi as u16) << 8)
}

pub fn word_hi(w: u16) -> u8 {
    (w >> 8).try_into().unwrap()
}

pub fn word_lo(w: u16) -> u8 {
    (w & 0x00FF).try_into().unwrap()
}

//#[derive(Debug)]
pub struct Mem {
    ram: Vec<u8>,
}

impl Mem {
    // TODO: Handle special regions.
    pub fn new(size: usize) -> Mem {
        Mem { ram: vec![0; size] }
    }
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

pub mod flag {
#[derive(PartialEq, Debug, Clone, Copy)]
    pub struct T(pub u8);
    impl std::ops::BitAnd for T {
	type Output = Self;
	fn bitand(self, rhs: Self) -> Self::Output {
	    Self(self.0 & rhs.0)
	}
    }
    pub const N: T = T(0x80); // negative
    pub const V: T = T(0x40); // overflow
    // bit 5 ignored
    pub const B: T = T(0x10); // break (fake flag, only appears when flags saved in RAM)
    pub const D: T = T(0x08); // decimal
    pub const I: T = T(0x04); // interrupt
    pub const Z: T = T(0x02); // zero
    pub const C: T = T(0x01); // carry
}

type CycleCount = usize;

pub struct Registers {
    pub a: u8,
    pub x: u8,
    pub y: u8,
    pub p: u8, // flags register
    pub s: u8, // stack pointer
    pub pc: u16, // program counter
    pub ic: usize, // instruction count
    pub cc: CycleCount, // cycle count
}

#[derive(Clone,Copy,FromPrimitive,PartialEq,Debug)]
pub enum Mnemonic {
    ILLEGAL = 0,
    ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK,
    BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX,
    DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR,
    NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC,
    SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS,
    TYA,
}

#[allow(non_upper_case_globals)]
pub mod addr_mode_flag {
#[derive(PartialEq, Debug, Clone, Copy)]
    pub struct T(pub u8);
    impl std::ops::BitAnd for T {
	type Output = Self;
	fn bitand(self, rhs: Self) -> Self::Output {
	    Self(self.0 & rhs.0)
	}
    }
    pub fn combine(f1: T, f2: T, f3: T) -> T {
        let T(v1) = f1;
        let T(v2) = f2;
        let T(v3) = f3;
        T(v1 | v2 | v3)
    }
    pub fn is_set(f1: T, f2: T) -> bool {
        let T(v1) = f1;
        let T(v2) = f2;
        (v1 & v2) != 0
    }
    pub const NoFlag  : T = T(0);
    pub const Resolve : T = T(1 << 0);
    //pub const TwoByte : T = T(1 << 1);
    pub const Ind     : T = T(1 << 2);
    pub const Abs     : T = T(1 << 3);
    pub const Zpg     : T = T(1 << 4);
    pub const X       : T = T(1 << 5);
    pub const Y       : T = T(1 << 6);
    pub const NoIndex : T = T(1 << 7);
}

#[derive(Clone,Copy,PartialEq,Debug)]
pub enum AddrMode {

  // treat A as implied

  Illegal,  // none (illegal instruction)

  Impl, // implied
  Imm,  // immediate

  Zpg,
  ZpgX,
  ZpgY,

  Rel,

  Abs,
  AbsX,
  AbsY,

  Ind,
  XInd,
  IndY,

}

/*
#[derive(PartialEq, PartialOrd, Debug, Clone, Copy)]
pub struct CycleCount(pub usize);

impl CycleCount {
    pub fn add(&self, cycles: usize) -> CycleCount {
        CycleCount(self.0 + cycles)
    }
}

impl std::ops::Sub for CycleCount {
    type Output = usize;
    fn sub(self, other: Self) -> Self::Output {
        self.0 - other.0
    }
}
*/

pub struct Cpu {
    pub reg: Registers,
}

impl Cpu {
    pub fn new(start_addr: u16) -> Cpu {
        let reg = Registers {
            a: 0, x: 0, y: 0, p: 0, s: 0xFF,
            pc: start_addr, ic: 0, cc: 0,
            //cc: CycleCount(0),
        };
        Cpu { reg: reg }
    }
    pub fn set_flag(&mut self, flag: flag::T, set: bool) {
        let flag::T(flag_value) = flag;
        if set {
            self.reg.p |= flag_value;
        } else {
            self.reg.p &= !(flag_value);
        }
    }
    pub fn get_flag(&self, flag: flag::T) -> bool {
        let flag::T(flag_value) = flag;
        0 != (self.reg.p & (flag_value))
    }
}

pub struct Machine {
    pub cpu: Cpu,
    pub mem: Mem,
}

