
extern crate sdl2;

mod shared;
mod cpu;
mod instructions;
mod sdl;

use crate::shared::{Mem, Cpu};

fn main() {
    let mut mem = Mem::new();
    let start_addr = mem.read_word(0xFFFC); // reset vector
    let mut cpu = Cpu::new(start_addr);
    let sdl = match sdl::init() {
        Ok(sdl) => sdl,
        Err(e) => { panic!(e) }
    };
    cpu::interp(&mut cpu, &mut mem);
}

fn main_loop(
    cpu: &mut Cpu,
    mem: &mut Mem,
)
{

}

