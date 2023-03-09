
extern crate sdl2;

mod shared;
mod cpu;
mod instructions;
mod sdl;

use crate::shared::{Mem, Cpu, C64_RAM_SIZE, C1541_RAM_SIZE};

#[allow(unused)]
fn main() {
    let mut mem = Mem::new(C64_RAM_SIZE);
    let start_addr = mem.read_word(0xFFFC); // reset vector
    let mut cpu = Cpu::new(start_addr);
    let mut sdl = match sdl::init() {
        Ok(sdl) => sdl,
        Err(e) => { panic!("Failed to init SDL. {}", e) }
    };
    main_loop(&mut cpu, &mut mem, &mut sdl);
}

fn main_loop(
    cpu: &mut Cpu,
    mem: &mut Mem,
    sdl: &mut sdl::Sdl,
)
{
    const CLOCK_FREQUENCY_HZ: f64 = 1e6; // 1 MHz
    const CYCLE_DUR_MS: f64 = 1000.0 / CLOCK_FREQUENCY_HZ;
    let mut time = std::time::Instant::now();
    loop {
        let cycle_count_before = cpu.reg.cc;
        cpu::interp_one_instruction(cpu, mem);
        let cycle_count_after = cpu.reg.cc;
        let cycles_elapsed = cycle_count_after - cycle_count_before;
        let end_time = (cycles_elapsed as f64) * CYCLE_DUR_MS;
        // disk drive catchup
        // video catchup
        // sound catchup
        // bus
        // render
    }
}

