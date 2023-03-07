
const RAM_SIZE: usize = 0x10000;

//#[derive(Debug)]
struct Memory {
    ram: [u8; RAM_SIZE],
}

impl Memory {
    // TODO: Handle special regions.
    pub fn read(&self, addr: u16) -> u8 {
        self.ram[addr as usize]
    }
    pub fn write(&mut self, addr: u16, val: u8) {
        self.ram[addr as usize] = val;
    }
}

fn main() {
    println!("Hello, world!");
}

