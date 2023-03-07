
const RAM_SIZE: usize = 0x10000;

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
}

enum Flag {
    N = 0x80, // negative
    V = 0x40, // overflow
    // bit 5 ignored
    B = 0x10, // break (fake flag, only appears when flags saved in RAM)
    D = 0x08, // decimal
    I = 0x04, // interrupt
    Z = 0x02, // zero
    C = 0x01, // carry
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

enum Instr {
    ADC=1, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK,
    BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX,
    DEY, EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR,
    NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC,
    SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS,
    TYA,
}

enum AddrModeFlag {
    Resolve = 1 << 0,
    //TwoByte = 1 << 1,
    Ind     = 1 << 2,
    Abs     = 1 << 3,
    Zpg     = 1 << 4,
    X       = 1 << 5,
    Y       = 1 << 6,
    NoIndex = 1 << 7,
}

enum AddrMode {

  Impl = 0x00, // implied
  Imm  = 0x01, // immediate
  //A    = 0x00, // treat A as implied

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

