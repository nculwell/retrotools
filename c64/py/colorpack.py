#!/usr/bin/env python3

import sys

def main():
    _, filename = sys.argv
    lines = read_file_lines(filename)
    #print(lines)
    for line in lines:
        #print("line", line)
        colors = parse_line(line)
        #print(colors)
        packed = pack_colors(colors)
        #print(packed)
        print(
            str_colors(colors),
            "  ",
            str_binary(packed),
            "  ",
            str_hex(packed)
        )

def parse_line(ln):
    assert(len(ln) == 8)
    return [ int(c) for c in ln ]

def pack_colors(colors):
    #print("colors", colors)
    octets = []
    for i in range(0, len(colors), 4):
        group = colors[i : i+4]
        packed = pack_octet(group)
        octets.append(packed)
    return octets

def pack_octet(colors4):
    #print("4colors", colors4)
    assert(len(colors4) == 4)
    octet = 0
    shift = 8
    for i in range(4):
        shift = shift - 2
        c = colors4[i]
        shifted = c << shift
        octet |= shifted
    #print("packed", octet)
    return octet

def str_colors(colors):
    return "".join(( "%d"%c for c in colors ))

def str_binary(packed):
    binary_prefixed = [ bin(x) for x in packed ]
    binary = [ b[2:] for b in binary_prefixed ]
    formatted = [ "%08d"%int(x) for x in binary ]
    return " ".join(formatted)

def str_hex(packed):
    return " ".join(( "%02X"%x for x in packed ))

def read_file_lines(filename):
    with open(filename, encoding="ascii") as f:
        return [ ln.strip() for ln in f.readlines() ]

if __name__ == "__main__":
    main()

