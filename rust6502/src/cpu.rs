
use crate::shared::*;
use crate::shared::Mnemonic::*;
use crate::shared::addr_mode_flag::is_set;

// Set the N and Z flags.
fn set_nz(cpu: &mut Cpu, byte_value: u8) {
    if byte_value == 0 {
        cpu.set_flag(flag::Z, true);
        cpu.set_flag(flag::N, false);
    } else {
        cpu.set_flag(flag::Z, false);
        cpu.set_flag(flag::N, 0 != (byte_value & 0x80));
    }
}

fn push(cpu: &mut Cpu, mem: &mut Mem, operand: u8) {
    if cpu.reg.s == 0 {
        panic!("Stack overflow.");
    }
    // traceStack(m, operand, '>');
    mem.write(0x100 + (cpu.reg.s as u16), operand);
    cpu.reg.s -= 1;
}

fn pull(cpu: &mut Cpu, mem: &Mem) -> u8 {
    if cpu.reg.s == 0xFF {
        panic!("Stack underflow.");
    }
    cpu.reg.s += 1;
    let v = mem.read(0x100 + (cpu.reg.s as u16));
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
    cpu.set_flag(flag::C, value & 0x80 != 0);
    let shifted = value << 1;
    set_nz(cpu, shifted);
    shifted
}

fn bitwise_lsr(cpu: &mut Cpu, value: u8) -> u8 {
    cpu.set_flag(flag::C, value & 1 != 0);
    let shifted = value >> 1;
    set_nz(cpu, shifted);
    shifted
}

fn bitwise_rol(cpu: &mut Cpu, value: u8) -> u8 {
    let carry_set_before = cpu.get_flag(flag::C);
    cpu.set_flag(flag::C, value & 0x80 != 0);
    let mut shifted = value << 1;
    if carry_set_before {
        shifted += 1; // set the low bit to 1
    }
    set_nz(cpu, shifted);
    shifted
}

fn bitwise_ror(cpu: &mut Cpu, value: u8) -> u8 {
    let carry_set_before = cpu.get_flag(flag::C);
    cpu.set_flag(flag::C, value & 1 != 0);
    let mut shifted = value >> 1;
    if carry_set_before {
        shifted |= 0x80; // set the high bit to 1
    }
    set_nz(cpu, shifted);
    shifted
}

// Implements ADC, SBC, CMP, CPX, CPY.
fn add(cpu: &mut Cpu, reg_val: u8, mem_val: u8, is_cmp: bool) {
    let mut diff: u16 = (reg_val as u16) + (mem_val as u16);
    // The carry flag always means +1 here.
    // This works for subtraction because we're subtracting the one's complement
    // instead of the two's complement. (The +1 from the carry flag is the
    // missing +1 to convert one's complement to two's complement.)
    if is_cmp || cpu.get_flag(flag::C) {
        diff += 1; // CMP behaves like SBC with carry set.
    }
    let b: u8 = diff as u8; // Truncate to 8 bits.
    // All instructions set N, Z and C.
    set_nz(cpu, b);
    cpu.set_flag(flag::C, diff & 0x100 != 0); // carry
    if !is_cmp {
        // ADC and SBC set A and V, but compare instructions don't.
        let overflow = 0 != ((reg_val ^ b) & (mem_val ^ b) & 0x80);
        cpu.set_flag(flag::V, overflow);
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

fn jump(cpu: &mut Cpu, _mem: &mut Mem, addr: u16, far: bool) {
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
    cpu: &mut Cpu,
    mem: &Mem,
    admd: AddrMode,
    admd_flags: addr_mode_flag::T,
) -> (bool, u16, u16)
{
    let mut is_indirect = false;
    let mut raw_addr: u16 = 0;
    let mut eff_addr: u16 = 0;
    let mut addr: u16 = mem.read(cpu.reg.pc) as u16;
    cpu.reg.pc += 1;
    if is_set(admd_flags, addr_mode_flag::Abs) {
        // Read second byte.
        addr |= (mem.read(cpu.reg.pc) as u16) << 8;
        cpu.reg.pc += 1;
        raw_addr = addr;
        // absolute
        if admd == AddrMode::AbsX {
            addr += cpu.reg.x as u16;
        } else if admd == AddrMode::AbsY {
            addr += cpu.reg.y as u16;
        } else {
            assert!(admd == AddrMode::Abs);
        }
    } else {
        if is_set(admd_flags, addr_mode_flag::Ind) {
            // indirect
            if admd == AddrMode::XInd {
                addr = mem.read_word(addr + cpu.reg.x as u16);
            } else {
                if admd == AddrMode::IndY {
                    addr = mem.read_word(addr);
                    addr += cpu.reg.y as u16;
                } else {
                    assert!(admd == AddrMode::Ind);
                    addr |= (mem.read(cpu.reg.pc) as u16) << 8;
                    cpu.reg.pc += 1;
                    raw_addr = addr;
                    addr = mem.read_word(addr);
                }
            }
            is_indirect = true;
        } else if admd == AddrMode::Rel {
            // relative: operand is a signed offset
            addr = ((cpu.reg.pc as i32) + ((addr as i8) as i32)) as u16;
        } else {
            // zero page
            assert!(is_set(admd_flags, addr_mode_flag::Zpg));
            if admd == AddrMode::ZpgX {
                addr += cpu.reg.x as u16;
            } else if admd == AddrMode::ZpgY {
                addr += cpu.reg.y as u16;
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
    mnemonic: Mnemonic,
    operand: u8,
)
{
    match mnemonic {

        // LOAD

        LDA => { cpu.reg.a = operand; set_nz(cpu, operand); }
        LDX => { cpu.reg.x = operand; set_nz(cpu, operand); }
        LDY => { cpu.reg.y = operand; set_nz(cpu, operand); }

        // ADD / SUB

        ADC => { add(cpu, cpu.reg.a, operand, false); }
        SBC => { add(cpu, cpu.reg.a, !operand, false); }
        CMP => { add(cpu, cpu.reg.a, !operand, true); }
        CPX => { add(cpu, cpu.reg.x, !operand, true); }
        CPY => { add(cpu, cpu.reg.y, !operand, true); }

        // BITWISE

        ORA => { cpu.reg.a |= operand; set_nz(cpu, cpu.reg.a); }
        AND => { cpu.reg.a &= operand; set_nz(cpu, cpu.reg.a); }
        EOR => { cpu.reg.a ^= operand; set_nz(cpu, cpu.reg.a); }

        // ILLEGAL OPCODE

        _ => { panic!("Unexpected instruction."); }

    }
}

fn interp_address(
    cpu: &mut Cpu,
    mem: &mut Mem,
    mnemonic: Mnemonic,
    addr: u16,
)
{
    match mnemonic {

        // JUMPS

        JSR => {
            let push_addr = cpu.reg.pc - 1; // JSR pushes return addr - 1
            push(cpu, mem, word_hi(push_addr));
            push(cpu, mem, word_lo(push_addr));
            jump(cpu, mem, addr, true);
        }
        JMP => { jump(cpu, mem, addr, true); }

        // BRANCHES

        BPL => { if !cpu.get_flag(flag::N) { jump(cpu, mem, addr, false); } }
        BMI => { if  cpu.get_flag(flag::N) { jump(cpu, mem, addr, false); } }
        BVS => { if  cpu.get_flag(flag::V) { jump(cpu, mem, addr, false); } }
        BCC => { if !cpu.get_flag(flag::C) { jump(cpu, mem, addr, false); } }
        BCS => { if  cpu.get_flag(flag::C) { jump(cpu, mem, addr, false); } }
        BNE => { if !cpu.get_flag(flag::Z) { jump(cpu, mem, addr, false); } }
        BEQ => { if  cpu.get_flag(flag::Z) { jump(cpu, mem, addr, false); } }

        // LOAD

        LDA => { let v = load(mem, addr); cpu.reg.a = v; set_nz(cpu, v); }
        LDX => { let v = load(mem, addr); cpu.reg.x = v; set_nz(cpu, v); }
        LDY => { let v = load(mem, addr); cpu.reg.y = v; set_nz(cpu, v); }

        // STORE

        STA => { store(mem, addr, cpu.reg.a); }
        STX => { store(mem, addr, cpu.reg.x); }
        STY => { store(mem, addr, cpu.reg.y); }
        BIT => { store(mem, addr, cpu.reg.p); } // TODO: brk flag

        // INCREMENT / DECREMENT (see also interp_implied)

        INC => { let v = store(mem, addr, mem.read(addr) + 1); set_nz(cpu, v); }
        DEC => { let v = store(mem, addr, mem.read(addr) - 1); set_nz(cpu, v); }

        // BIT SHIFTS

        ASL => { mem.write(addr, bitwise_asl(cpu, mem.read(addr))); }
        LSR => { mem.write(addr, bitwise_lsr(cpu, mem.read(addr))); }
        ROL => { mem.write(addr, bitwise_rol(cpu, mem.read(addr))); }
        ROR => { mem.write(addr, bitwise_ror(cpu, mem.read(addr))); }

        // The operations with immediate arguments can be applied to
        // memory just by loading the value from memory and using the
        // same mnemonic.
        _ => { interp_immediate(cpu, mnemonic, mem.read(addr)); }

    }
}

fn interp_implied(
    cpu: &mut Cpu,
    mem: &mut Mem,
    mnemonic: Mnemonic,
)
{
    match mnemonic {

        // RETURN

        RTS => {
            if cpu.reg.s > 0xFD {
                panic!("Stack underflow in RTS.");
            }
            return_from_sub(cpu, mem);
        }

        // STACK

        PHP => { push(cpu, mem, cpu.reg.p); }
        PLP => { cpu.reg.p = pull(cpu, mem); }
        PHA => { push(cpu, mem, cpu.reg.a); }
        PLA => { cpu.reg.a = pull(cpu, mem); }

        // FLAGS

        CLC => { cpu.set_flag(flag::C, false); }
        SEC => { cpu.set_flag(flag::C, true); }
        CLV => { cpu.set_flag(flag::V, false); }
        CLD => { cpu.set_flag(flag::D, false); }
        SED => { cpu.set_flag(flag::D, true); }
        CLI => { cpu.set_flag(flag::I, false); }
        SEI => { cpu.set_flag(flag::I, true); }

        // INCREMENT / DECREMENT (see also interp_address)

        INX => { let v = cpu.reg.x + 1; cpu.reg.x = v; set_nz(cpu, v); }
        DEX => { let v = cpu.reg.x - 1; cpu.reg.x = v; set_nz(cpu, v); }
        INY => { let v = cpu.reg.y + 1; cpu.reg.y = v; set_nz(cpu, v); }
        DEY => { let v = cpu.reg.y - 1; cpu.reg.y = v; set_nz(cpu, v); }

        // TRANSFER REGISTERS

        TYA => { let v = cpu.reg.y; cpu.reg.a = v; set_nz(cpu, v); }
        TAY => { let v = cpu.reg.a; cpu.reg.y = v; set_nz(cpu, v); }
        TXA => { let v = cpu.reg.x; cpu.reg.a = v; set_nz(cpu, v); }
        TAX => { let v = cpu.reg.a; cpu.reg.x = v; set_nz(cpu, v); }
        TXS => { cpu.reg.s = cpu.reg.x; /* This instruction doesn't set N/Z */ }
        TSX => { let v = cpu.reg.s; cpu.reg.x = v; set_nz(cpu, v); }

        // BIT SHIFTS

        ASL => { cpu.reg.a = bitwise_asl(cpu, cpu.reg.a); }
        LSR => { cpu.reg.a = bitwise_lsr(cpu, cpu.reg.a); }
        ROL => { cpu.reg.a = bitwise_rol(cpu, cpu.reg.a); }
        ROR => { cpu.reg.a = bitwise_ror(cpu, cpu.reg.a); }

        // NO-OP

        NOP => { }

        // TODO: Currently not supporting these because I haven't implemented interrupts.

        BRK | RTI => { panic!("Interrupt-related opcodes not supported."); }

        // ILLEGAL OPCODE

        _ => { panic!("Unexpected instruction."); }

    }
}

pub fn interp_one_instruction(
    cpu: &mut Cpu,
    mem: &mut Mem,
)
{

    use crate::instructions::*;

        // Read the opcode.
        let opcode_addr = cpu.reg.pc;
        let opcode = mem.read(opcode_addr);

        // Advance PC and increment the Instruction Counter.
        cpu.reg.pc += 1;
        cpu.reg.ic += 1;

        // Decode the instruction.

        let instr: Instruction = instruction_lookup(opcode);
        if instr.mnemonic == ILLEGAL {
            panic!("Illegal instruction: {:02X} (PC={:04X}, IC={:X})",
            opcode, opcode_addr, cpu.reg.ic);
        }
        let admd_flags = lookup_addr_mode_flags(instr.addr_mode);
        let mut is_indirect: bool = false;
        let mut operand: u16 = 0;
        let mut raw_operand: u16 = u16::MAX;

        if is_set(admd_flags, addr_mode_flag::Resolve) {
            (is_indirect, operand, raw_operand) =
                resolve_address(cpu, mem, instr.addr_mode, admd_flags);
        } else {
            if instr.addr_mode == AddrMode::Imm {
                operand = mem.read(cpu.reg.pc) as u16;
                cpu.reg.pc += 1;
            } else {
                assert!(instr.addr_mode == AddrMode::Impl);
            }
        }

        // trace_instruction(cpu, mem,
        //   opcode_addr, instr.mnemonic, admd, admdFlags, operand, rawOperand);

        // Execute.

        match instr.addr_mode {
            AddrMode::Impl => { interp_implied(cpu, mem, instr.mnemonic); }
            AddrMode::Imm  => { interp_immediate(cpu, instr.mnemonic, operand as u8); }
            _              => { interp_address(cpu, mem, instr.mnemonic, operand); }
        }

}

pub fn interp(
    cpu: &mut Cpu,
    mem: &mut Mem,
)
{
    // prepareHooks
    loop {
        interp_one_instruction(cpu, mem);
    }
}

