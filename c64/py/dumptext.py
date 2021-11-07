#!/usr/bin/env python3

import os, sys
import traceback
import base40

LINE_BYTES = 32

def main():
    filename = sys.argv[1]
    with open(filename, "rb") as f:
        content = f.read()
    argcount = len(sys.argv)
    if argcount > 2:
        begin = sys.argv[2]
    else:
        begin = "0"
    if argcount > 3:
        end = sys.argv[3]
    else:
        end = "FFFFFF"
    if argcount > 4:
        raise Exception("Too many arguments.")
    b = int(begin, 16)
    e = int(end, 16)
    assert b >= 0
    if e >= len(content):
        e = len(content) - 1
    i = b - (b % LINE_BYTES)
    while i <= e:
        print_line(content, i, b, e)
        i += LINE_BYTES

def print_line(content, base, b, e):
    hexes = []
    line_ascii_chars = []
    c64screen_chars = []
    for i in range(LINE_BYTES):
        j = base + i
        if j < b or j > e:
            hexes.append("  ")
            line_ascii_chars.append(" ")
        else:
            character = content[j]
            c = int(character)
            hexes.append("%02x" % c)
            ascii_char = get_ascii_char(c)
            c64screen_char = get_c64screen_char(c)
            line_ascii_chars.append(ascii_char)
            c64screen_chars.append(c64screen_char)
    b40_chars = []
    for i in range(0, LINE_BYTES, 2):
        j = base + i
        bs = content[j:j+2]
        try:
            decoded = base40.base40_decode(bs)
        except:
            #traceback.print_exc()
            decoded = "<E>"
        b40_chars += decoded
    #chars_column_1 = c64screen_chars
    chars_column_1 = c64screen_chars
    chars_column_2 = line_ascii_chars
    print("%06X:" % base,
          *[
              " %s" % space16(" ".join(hexes)),
              #" %s" % "".join(chars_column_1),
              " %s" % "".join(chars_column_2),
          ])

def space16(hex_text):
    if LINE_BYTES == 32:
        midpoint = int(len(hex_text)/2)
        return hex_text[:midpoint] + " " + hex_text[midpoint:]

def get_ascii_char(c):
    cx = c & 0x7F
    if c == 0:
        ch = "."
    elif cx >= 0x20 and cx < 0x7F:
        ch = chr(cx)
    else:
        ch = "~"
    return ch

def get_c64screen_char(c):
    if c < 0x20:
        return get_ascii_char(64 + c)
    else:
        return get_ascii_char(c)

if __name__ == "__main__":
    main()

