#!/usr/bin/env python3

import os, sys

with open("FILENAME", encoding="ascii") as f:
    FILENAME = f.read().strip()
LINE_BYTES = 32

def main():
    argcount = len(sys.argv)
    if argcount > 1:
        begin = sys.argv[1]
    else:
        begin = "0"
    if argcount > 2:
        end = sys.argv[2]
    else:
        end = "FFFFFF"
    if argcount > 3:
        raise Exception("Too many arguments.")
    b = int(begin, 16)
    e = int(end, 16)
    assert b >= 0
    i = b - (b % LINE_BYTES)
    while i <= e:
        print_line(i, b, e)
        i += LINE_BYTES

def print_line(base, b, e):
    hexes = []
    chars = []
    i = 0
    while i < LINE_BYTES:
        j = base + i
        if j < b or j > e:
            hexes.append("  ")
            chars.append(" ")
        else:
            c = int(j)
            hexes.append("%02x" % c)
            ch = chr(c)
            chars.append(ch)
        i += 1
    print("%06X:  %s" % (base,
                            # " ".join(hexes),
                             "".join(chars)))

if __name__ == "__main__":
    main()

