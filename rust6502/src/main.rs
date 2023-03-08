
mod shared;
mod cpu;
mod instructions;

use crate::shared::{Mem, Cpu};

fn main() {
    let mut mem = Mem::new();
    let start_addr = mem.read_word(0xFFFC); // reset vector
    let mut cpu = Cpu::new(start_addr);
    cpu::interp(&mut cpu, &mut mem);
}

