#!/usr/bin/python3

ITBL = [
0x3c,
0x92,
0x3f,
0x67,
0x75,
0x5d,
0xb5,
0xc6,
0x57,
0xaa,
0x81,
0xc1,
0xdb,
0xde,
0xe1,
0xe4,
0xe7,
0xea,
0xed,
]

addr = 0xC534

for x in ITBL:
    print("%04X: %02X -> %02X -> %04X" % (addr, x, x+0x47, 0xc500+x+0x47))
    addr += 1

