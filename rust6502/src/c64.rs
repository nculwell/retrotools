// vim: et ts=8 sts=4 sw=4

use std::fs;

const KB: usize = 1024;
const RAM_SIZE: usize = 64 * KB;
const COLOR_RAM_SIZE: usize = 1 * KB;
const VIC2_REG_LENGTH: usize = 0x2F;
const CIA_REG_LENGTH: usize = 0x10;
const ROM_PATH: &str = "c64/rom";
const CHARGEN_ROM_SIZE: usize = 4 * KB;
const BASIC_ROM_SIZE: usize = 0x2000;
const KERNAL_ROM_SIZE: usize = 0x2000;

struct HardwareC64 {
    ram: [mut u8; RAM_SIZE],
    color_ram: [mut u8; COLOR_RAM_SIZE], // only the lower 4 bits of each byte are used
    vic2_reg: [mut u8; VIC2_REG_LENGTH],
    cia_reg: [[mut u8; CIA_REG_LENGTH]; 2],
    chargen_rom: [u8; CHARGEN_ROM_SIZE],
    basic_rom: [u8; BASIC_ROM_SIZE],
    kernal_rom: [u8; KERNAL_ROM_SIZE],
}

impl HardwareC64 {

    fn init(&self) -> io::Result<&mut HardwareC64> {
        let chargen_rom = self.rom_load("chargen.rom")?;
        let basic_rom = self.rom_load("basic.rom")?;
        let kernal_rom = self.rom_load("kernal.rom")?;
        self.video_init();
        HardwareC64 {
            ram: [0; RAM_SIZE],
            color_ram: [0; COLOR_RAM_SIZE],
            vic2_reg: [0; VIC2_REG_LENGTH],
            cia_reg: [[0; CIA_REG_LENGTH]; 2],
            chargen_rom: chargen_rom,
            basic_rom: basic_rom,
            kernal_rom, kernal_rom,
        }
    }

    fn rom_load(&self, rom_filename: &str) -> io::Result<Vec<u8>> {
        let path = format!("{}/{}", ROM_PATH, rom_filename);
        let rom = fs::read(path)?;
        Ok(path)
    }

    fn video_init(&self) {
        self.vic2_reg = [0; VIC2_REG_LENGTH];
        self.vic2_reg[0x11] = 0b00011011;
        self.vic2_reg[0x16] = 0b11001000;
    }

    fn video_write(&self, addr: u16, val: u8) {
        let conv_addr = video_addr_conv(addr);
        self.vic2_reg[conv_addr] = val;
    }

    fn video_read(&self, addr: u16) {
        let conv_addr = video_addr_conv(addr);
        self.vic2_reg[conv_addr]
    }

    fn cia_write(&self, cia_number: usize, addr: u16, val: u8) {
        // TODO
        let conv_addr = cia_addr_conv(cia_number, addr);
        self.cia_reg[cia_number-1][conv_addr] = val;
    }

    fn cia_read(&self, cia_number: usize, addr: u16) {
        // TODO
        let conv_addr = cia_addr_conv(cia_number, addr);
        self.cia_reg[cia_number-1][conv_addr]
    }

}

fn video_addr_conv(addr: u16) -> usize {
    if addr < 0xD000 || addr > 0xDFFF {
        panic!("Video address out of range: {04X}", addr);
    }
    let converted_addr = ((addr as usize) - 0xD000) & 0x3F;
    if converted_addr >= 0x2F {
        panic!("Unusable video memory: {04X}", addr);
    }
    converted_addr
}

fn cia_addr_conv(cia_number: usize, addr: u16) -> usize {
    //assert!(cia_number == 1 || cia_number == 2);
    let range_start: usize = match cia_number {
        1 => 0xDD00,
        2 => 0xDC00,
        _ => { panic!("Invalid CIA number: {}", cia_number) }
    }
    if addr < range_start || addr >= (range_start + 0x1000) {
        panic!("CIA address out of range: {04X}", addr);
    }
    ((addr as usize) - range_start) & 0x0F
}

fn render(hw: HardwareC64) {

    // Figure out video setup.

    // $DD00 Port A, serial bus access. Bits:
    //     Bits #0-#1: VIC bank. Values:
    //         %00, 0: Bank #3, $C000-$FFFF, 49152-65535.
    //         %01, 1: Bank #2, $8000-$BFFF, 32768-49151.
    //         %10, 2: Bank #1, $4000-$7FFF, 16384-32767.
    //         %11, 3: Bank #0, $0000-$3FFF, 0-16383.
    let bank_bits = hw.cia_reg[2][0] & 0b0011;
    let bank_base_addr: u16 = match bank_bits {
        0 => 0xC000,
        1 => 0x8000,
        2 => 0x4000,
        3 => 0x0000,
        _ => { panic!("Invalid bank bits: {}", bank_bits); }
    }

    // $D011 Screen control register #1. Bits:
    //   Bits #0-#2: Vertical raster scroll.
    //   Bit #3: Screen height; 0 = 24 rows; 1 = 25 rows.
    //   Bit #4:
    //     0 = Screen off, complete screen is covered by border;
    //     1 = Screen on, normal screen contents are visible.
    //   Bit #5: 0 = Text mode; 1 = Bitmap mode.
    //   Bit #6: 1 = Extended background mode on.
    //   Bit #7:
    //     Read: Current raster line (bit #8).
    //     Write: Raster line to generate interrupt at (bit #8).
    let bitmap_mode = 0 != (hw.vic2_reg[0x11] & (1 << 5));
    let extbg_mode = 0 != (hw.vic2_reg[0x11] & (1 << 6));

    // $D016 Screen control register #2. Bits:
    //   Bits #0-#2: Horizontal raster scroll.
    //   Bit #3: Screen width; 0 = 38 columns; 1 = 40 columns.
    //   Bit #4: 1 = Multicolor mode on.
    let multicolor_mode = 0 != (hw.vic2_reg[0x16] & (1 << 4));

    // $D018 Memory setup register.
    let mem_setup_reg = hw.vic2_reg[0x18];
    let (video_rel_addr: u16, use_chargen: bool) = {
        if !bitmap_mode {
            // Text mode
            // Bits #1-#3: In text mode, pointer to character memory (bits #11-#13),
            // relative to VIC bank
            let text_mode_bits = (mem_setup_reg >> 1) & 0b111;
            let rel_addr = (text_mode_bits as u16) * 0x800;
            // Values %010 and %011 in VIC bank #0 and #2 select Character ROM instead.
            let use_chargen: bool =
                (bank_bits == 0 || bank_bits == 2)
                && (text_mode_bits == 0b010 || text_mode_bits == 0b011);
            (rel_addr, use_chargen)
        } else {
            // Bitmap mode
            // In bitmap mode, pointer to bitmap memory (bit #13), relative to VIC bank
            let bitmap_bit = (mem_setup_reg >> 3) & 1;
            let rel_addr = (bitmap_bit as u16) * 0x2000;
            (rel_addr, false)
        }
    }
    let video_base_addr = bank_base_addr + video_rel_addr;
    // Bits #4-#7: Pointer to screen memory (bits #10-#13), relative to VIC bank
    let screen_ptr_bits = (mem_setup_reg >> 4) & 0b1111;
    let screen_mem_rel_addr = (screen_ptr_bits as u16) * 0x0400;
    let screen_mem_addr = bank_base_addr + screen_mem_rel_addr;

    let border_color = hw.vic2_reg[0x20]; // only bits 0-3
    let bg_color = hw.vic2_reg[0x21]; // only bits 0-3

    // $D018 Memory setup register.
    //
    // Sprites
    let sprite_enabled_reg = hw.vic2_reg[0x15];
    if sprite_enabled_reg != 0 {
        const sprites_base_addr: usize = 0;
        const sprites_color_base_addr: usize = 0x27;
        let sprite_dbl_hght_reg = hw.vic2_reg[0x17];
        let sprite_x_coord_bit8_reg = hw.vic2_reg[0x10];
        let sprite_extra_color_1 = hw.vic2_reg[0x25];
        let sprite_extra_color_2 = hw.vic2_reg[0x26];
        for i in 0..8 {
            if 0 == (sprite_enabled_reg & (1 << i)) {
                continue; // sprite is disabled
            }
            let double_height = 0 != (sprite_dbl_hght_reg & (1 << i));
            let sprite_x_addr = sprites_base_addr + (2 * i);
            let sprite_y_addr = sprite_x_addr + 1;
            let sprite_x_coord =
                hw.vic2_reg[sprite_y_addr]
                + (((sprite_x_coord_bit8_reg >> i) & 1) << 8);
            let sprite_y_coord = hw.vic2_reg[sprite_y_addr];
            let sprite_color = hw.vic2_reg[sprites_color_base_addr + i];
            // render this sprite
        }
    }

}

// $D018 Memory setup register. Bits:
//     Bits #1-#3: In text mode, pointer to character memory (bits #11-#13),
//          relative to VIC bank, memory address $DD00. Values:
//         %000, 0: $0000-$07FF, 0-2047.
//         %001, 1: $0800-$0FFF, 2048-4095.
//         %010, 2: $1000-$17FF, 4096-6143.
//         %011, 3: $1800-$1FFF, 6144-8191.
//         %100, 4: $2000-$27FF, 8192-10239.
//         %101, 5: $2800-$2FFF, 10240-12287.
//         %110, 6: $3000-$37FF, 12288-14335.
//         %111, 7: $3800-$3FFF, 14336-16383.
//     Values %010 and %011 in VIC bank #0 and #2 select Character ROM instead.
//     In bitmap mode, pointer to bitmap memory (bit #13),
//          relative to VIC bank, memory address $DD00. Values:
//         %0xx, 0: $0000-$1FFF, 0-8191.
//         %1xx, 4: $2000-$3FFF, 8192-16383.
//     Bits #4-#7: Pointer to screen memory (bits #10-#13),
//          relative to VIC bank, memory address $DD00. Values:
//         %0000, 0: $0000-$03FF, 0-1023.
//         %0001, 1: $0400-$07FF, 1024-2047.
//         %0010, 2: $0800-$0BFF, 2048-3071.
//         %0011, 3: $0C00-$0FFF, 3072-4095.
//         %0100, 4: $1000-$13FF, 4096-5119.
//         %0101, 5: $1400-$17FF, 5120-6143.
//         %0110, 6: $1800-$1BFF, 6144-7167.
//         %0111, 7: $1C00-$1FFF, 7168-8191.
//         %1000, 8: $2000-$23FF, 8192-9215.
//         %1001, 9: $2400-$27FF, 9216-10239.
//         %1010, 10: $2800-$2BFF, 10240-11263.
//         %1011, 11: $2C00-$2FFF, 11264-12287.
//         %1100, 12: $3000-$33FF, 12288-13311.
//         %1101, 13: $3400-$37FF, 13312-14335.
//         %1110, 14: $3800-$3BFF, 14336-15359.
//         %1111, 15: $3C00-$3FFF, 15360-16383.

