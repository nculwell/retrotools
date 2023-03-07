
let x = 123;

fn set_nz(cpu: &mut Cpu, mem: &mut Mem, byteValue: u8) {
    if byteValue == 0 {
        cpu.set_flag(Flag::Z, true);
        cpu.set_flag(Flag::N, false);
    } else {
        set_flag(Flag::Z, false);
        set_flag(Flag::N, byteValue & 0x80);
    }
}

fn push(cpu: &mut Cpu, mem: &mut Mem, operand: u8) {
    if (cpu.reg.s == 0)
        panic!("Stack overflow.");
    // traceStack(m, operand, '>');
    mem.set(0x100 + cpu.reg.s, operand);
    cpu.reg.s -= 1;
}

fn pull(cpu: &mut Cpu, mem: &Mem) {
    if (cpu.reg.s == 0xFF)
        panic!("Stack underflow.");
    cpu.reg.s += 1;
    let v = mem.get(0x100 + cpu.reg.s);
    // traceStack(m, operand, '>');
    v
}

fn load(cpu: &mut Cpu, mem: &mut Mem, addr: u16) {
    let value = mem.get(addr);
    // trace(m, true, "LOAD %04X: %02X", addr, value);
    value
}

fn store(mem: &mut Mem, addr: u16, value: u8) {
    // trace(m, true, "STORE %04X: %02X -> %02X", addr, m->ram[addr], value);
    mem.set(addr, value);
    value
}

fn bitwise_asl(cpu: &mut Cpu, mem: &mut Mem, value: u8) {
    cpu.set_flag(Flag::C, value & 0x80 != 0);
    value <<= 1;
    set_nz(cpu, mem, value);
    value
}

fn bitwise_lsr(cpu: &mut Cpu, mem: &mut Mem, value: u8) {
    cpu.set_flag(Flag::C, value & 1 != 0);
    value >>= 1;
    set_nz(cpu, mem, value);
    value
}

fn bitwise_rol(cpu: &mut Cpu, mem: &mut Mem, value: u8) {
    let carry_set_before = cpu.get_flag(cpu, Flag::C);
    cpu.set_flag(Flag::C, value & 0x80 != 0);
    value <<= 1;
    if carry_set_before {
        value++; // set the low bit to 1
    }
    set_nz(cpu, mem, value);
    value
}

fn bitwise_ror(cpu: &mut Cpu, mem: &mut Mem, value: u8) {
    let carry_set_before = cpu.get_flag(cpu, Flag::C);
    cpu.set_flag(Flag::C, value & 1 != 0);
    value >>= 1;
    if carry_set_before {
        value |= 0x80; // set the high bit to 1
    }
    set_nz(cpu, mem, value);
    value
}

// Implements ADC, SBC and CMP.
fn add(cpu: &mut Cpu, mem: &mut Mem, reg_val: u8, mem_val: u8, is_cmp: bool) {
    let diff: u16 = reg_val + mem_val;
    // The carry flag always means +1 here.
    // This works for subtraction because we're subtracting the one's complement
    // instead of the two's complement. (The +1 from the carry flag is the
    // missing +1 to convert one's complement to two's complement.)
    if is_cmp || get_flag(m, FLAG_C) {
        diff++; // CMP behaves like SBC with carry set.
    }
    let b: u8 = diff; // Truncate to 8 bits.
    // All instructions set N, Z and C.
    set_nz(cpu, mem, b);
    cpu.set_flag(Flag::C, diff & 0x100 != 0); // carry
    if !is_cmp {
        // ADC and SBC set A and V, but compare instructions don't.
        let overflow = 0 != ((reg_val ^ b) & (mem_val ^ b) & 0x80);
        cpu.set_flag(Flag::V, overflow);
        cpu.reg.a = b;
    }
}

fn return_from_sub(cpu: &mut Cpu, mem: &Mem) {
    let return_addr_lo = pull(cpu, mem);
    let return_addr_hi = pull(cpu, mem);
    // The +1 here corrects for how JSR pushes addresses.
    let return_addr = 1 + word(return_addr_lo, return_addr_hi);
    // trace_set_pc(return_addr);
    cpu.reg.pc = return_addr;
}

fn jump(cpu: &mut Cpu, mem: &mut Mem, addr: u16, bool far) {
    // trace_set_pc(addr)
    // if (far && addr >= 0xF000) {
    //   emulateC64ROM(m, addr);
    //   returnFromSub(m);
    // } else {
    cpu.reg.pc = addr;
}

// RESOLVE ADDRESSING MODES


