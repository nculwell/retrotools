
fn set_nz(cpu: &mut Cpu, byteValue: u8) {
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
    mem.write(0x100 + cpu.reg.s, operand);
    cpu.reg.s -= 1;
}

fn pull(cpu: &mut Cpu, mem: &Mem) {
    if (cpu.reg.s == 0xFF)
        panic!("Stack underflow.");
    cpu.reg.s += 1;
    let v = mem.read(0x100 + cpu.reg.s);
    // traceStack(m, operand, '>');
    v
}

fn load(mem: &mut Mem, addr: u16) -> u8 {
    let value = mem.read(addr);
    // trace(m, true, "LOAD %04X: %02X", addr, value);
    value
}

fn store(mem: &mut Mem, addr: u16, value: u8) -> u8 {
    // trace(m, true, "STORE %04X: %02X -> %02X", addr, m->ram[addr], value);
    mem.write(addr, value);
    value
}

fn bitwise_asl(cpu: &mut Cpu, value: u8) -> u8 {
    cpu.set_flag(Flag::C, value & 0x80 != 0);
    value <<= 1;
    set_nz(cpu, value);
    value
}

fn bitwise_lsr(cpu: &mut Cpu, value: u8) -> u8 {
    cpu.set_flag(Flag::C, value & 1 != 0);
    value >>= 1;
    set_nz(cpu, value);
    value
}

fn bitwise_rol(cpu: &mut Cpu, value: u8) -> u8 {
    let carry_set_before = cpu.get_flag(cpu, Flag::C);
    cpu.set_flag(Flag::C, value & 0x80 != 0);
    value <<= 1;
    if carry_set_before {
        value++; // set the low bit to 1
    }
    set_nz(cpu, value);
    value
}

fn bitwise_ror(cpu: &mut Cpu, value: u8) -> u8 {
    let carry_set_before = cpu.get_flag(cpu, Flag::C);
    cpu.set_flag(Flag::C, value & 1 != 0);
    value >>= 1;
    if carry_set_before {
        value |= 0x80; // set the high bit to 1
    }
    set_nz(cpu, value);
    value
}

// Implements ADC, SBC, CMP, CPX, CPY.
fn add(cpu: &mut Cpu, reg_val: u8, mem_val: u8, is_cmp: bool) {
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
    set_nz(cpu, b);
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
    // }
}

// RESOLVE ADDRESSING MODES

fn deref(mem: &Mem, pointer: u16) -> u16 {
    mem.read_word(pointer)
}

fn resolve_address(
    cpu: &Cpu,
    mem: &Mem,
    admd: AddrMode,
    admd_flags: AddrModeFlags,
) -> (bool, u16, u16)
{
    let mut is_indirect = false;
    let mut raw_addr: u16 = 0;
    let mut eff_addr: u16 = 0;
    let mut addr: u16 = mem.read(cpu.reg.pc);
    cpu.reg.pc += 1;
    if admd_flags & AddrModeFlag::Abs {
        // Read second byte.
        addr |= mem.read(cpu.reg.pc) << 8;
        cpu.reg.pc += 1;
        raw_addr = addr;
        // absolute
        if admd == AddrMode::AbsX {
            addr += cpu.reg.x;
        } else if admd == AddrMode::AbsY {
            addr += cpu.reg.y;
        } else {
            assert!(admd == AddrMode::Abs);
        }
    } else {
        if admd_flags & AddrModeFlag::Ind {
            // indirect
            if admd == AddrMode::XInd {
                addr = mem.read_word(addr + cpu.reg.x);
            } else {
                if admd == AddrMode::IndY {
                    addr = mem.read_word(addr);
                    addr += cpu.reg.y;
                } else {
                    assert!(admd == AddrMode::Ind);
                    addr |= mem.read(cpu.reg.pc) << 8;
                    cpu.reg.pc += 1;
                    raw_addr = addr;
                    addr = mem.read_word(addr);
                }
            }
            is_indirect = true;
        } else if admd == AddrMode::Rel {
            // relative: operand is a signed offset
            addr = cpu.reg.pc + (addr as i8);
        } else {
            // zero page
            if 0 == (admd_flags & AddrModeFlags::Zpg) {
                // fprintf(stderr, "%s\n", addrModeInfo[admd].name);
            }
            assert!(0 != admd_flags & AddrModeFlags::Zpg);
            if admd == AddrMode::ZpgX {
                addr += cpu.reg.x;
            } else if admd == AddrMode::ZpgY {
                addr += cpu.reg.y;
            } else {
                assert!(admd == AddrMode::Zpg);
            }
        }
    }
    eff_addr = addr;
    (is_indirect, eff_addr, raw_addr)
}

fn interp_immediate(
    cpu: &mut Cpu,
    instruction: Instruction,
    operand: u8,
)
{
    match instruction {

        // LOAD

        LDA => { cpu.reg.a = operand; set_nz(cpu, operand); }
        LDX => { cpu.reg.x = operand; set_nz(cpu, operand); }
        LDA => { cpu.reg.y = operand; set_nz(cpu, operand); }

        // ADD / SUB

        ADC => { add(cpu, cpu.reg.a, operand, false); }
        SBC => { add(cpu, cpu.reg.a, !operand, false); }
        CMP => { add(cpu, cpu.reg.a, !operand, true); }
        CPY => { add(cpu, cpu.reg.x, !operand, true); }
        CPY => { add(cpu, cpu.reg.y, !operand, true); }

        // BITWISE

        ORA => { cpu.reg.a |= operand; set_nz(cpu, cpu.reg.a); }
        AND => { cpu.reg.a &= operand; set_nz(cpu, cpu.reg.a); }
        EOR => { cpu.reg.a ^= operand; set_nz(cpu, cpu.reg.a); }

        _ => panic!("Unexpected instruction.");

    }
}

fn interp_addr(
    cpu: &mut Cpu,
    mem: &mut Mem,
    instruction: Instruction,
    addr: u16,
)
{
    match instruction {

        // JUMPS

        JSR => {
            let push_addr = cpu.reg.pc - 1; // JSR pushes return addr - 1
            push(cpu, mem, word_hi(push_addr));
            push(cpu, mem, word_lo(push_addr));
            jump(cpu, mem, true);
        }
        JMP => { jump(cpu, mem, true); }

        // BRANCHES

        BPL => { if !cpu.get_flag(Flag::N) { jump(cpu, mem, addr, false); } }
        BMI => { if  cpu.get_flag(Flag::N) { jump(cpu, mem, addr, false); } }
        BVS => { if  cpu.get_flag(Flag::V) { jump(cpu, mem, addr, false); } }
        BCC => { if !cpu.get_flag(Flag::C) { jump(cpu, mem, addr, false); } }
        BCS => { if  cpu.get_flag(Flag::C) { jump(cpu, mem, addr, false); } }
        BNE => { if !cpu.get_flag(Flag::Z) { jump(cpu, mem, addr, false); } }
        BEQ => { if  cpu.get_flag(Flag::Z) { jump(cpu, mem, addr, false); } }

        // LOAD

        LDA => { let v = load(mem, addr); cpu.reg.a = v; set_nz(cpu, v); }
        LDX => { let v = load(mem, addr); cpu.reg.x = v; set_nz(cpu, v); }
        LDY => { let v = load(mem, addr); cpu.reg.y = v; set_nz(cpu, v); }

        // STORE

        STA => { store(mem, cpu.reg.a, addr); }
        STX => { store(mem, cpu.reg.x, addr); }
        STY => { store(mem, cpu.reg.y, addr); }
        BIT => { store(mem, cpu.reg.p, addr); } // TODO: brk flag

        // INCREMENT / DECREMENT

        INC => { let v = store(mem, mem.read(addr) + 1, addr); set_nz(cpu, v); }
        DEC => { let v = store(mem, mem.read(addr) - 1, addr); set_nz(cpu, v); }

        // BIT SHIFTS

        ASL => { mem.write(addr, bitwise_asl(cpu, mem.read(addr))); }
        LSR => { mem.write(addr, bitwise_lsr(cpu, mem.read(addr))); }
        ROL => { mem.write(addr, bitwise_rol(cpu, mem.read(addr))); }
        ROR => { mem.write(addr, bitwise_ror(cpu, mem.read(addr))); }

        // The opcodes with immediate arguments can be applied to memory just
        // by loading the value from memory and using the same opcode.
        _ => interp_immediate(cpu, mem.read(addr));

    }
}

