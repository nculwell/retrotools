#!/usr/bin/env python3

def mask_7bit(b): return b & 0b01111111
def mask_6bit(b): return b & 0b00111111
def mask_5bit(b): return b & 0b00011111
def mask_4bit_lo(b): return b & 0b00001111
def mask_4bit_hi(b): return (b & 0b11110000) >> 4
def mask_2bit_lo(b): return (b & 0b0011)

def word(lo, hi): return lo | (hi << 8)

def bitflag(bitnumber, b):
    assert 1 <= bitnumber <= 8
    return 0 != b & (1 << (bitnumber-1))

def test_bitflag():
    assert     bitflag(1, 0b00000001)
    assert not bitflag(1, 0b11111110)
    assert     bitflag(2, 0b00000010)
    assert not bitflag(2, 0b11111101)
    assert     bitflag(3, 0b00000100)
    assert not bitflag(3, 0b11111011)
    assert     bitflag(4, 0b00001000)
    assert not bitflag(4, 0b11110111)

