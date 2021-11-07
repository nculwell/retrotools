#!/usr/bin/env python3

import os, sys

class InvocationException(Exception):
    pass

def main():
    _, filename, target = sys.argv
    if not os.path.exists(filename):
        raise InvocationException("FILE NOT FOUND: "+filename)
    print("FILENAME:", filename)
    pattern = build_pattern(target)
    #print(pattern)
    with open(filename, "rb") as f:
        content = f.read()
    for i in range(0, len(content) - len(target) + 1):
        m = match_pattern(content, i, pattern)
        if m:
            sequence = [ int(x) for x in content[i:i+len(target)] ]
            print("%06X:" % i, to_hex(sequence), to_c64(sequence), to_asc(sequence))
            pass

def to_hex(sequence):
    return " ".join([ "%02x" % x for x in sequence ])

def to_c64(sequence):
    return "".join([ chr_c64(x) for x in sequence ])

def chr_c64(x):
    return chr(64+x if x < 0x20 else ord(chr_asc(x)))

def to_asc(sequence):
    return "".join([ chr_asc(x) for x in sequence ])

def chr_asc(x):
    if x >= 0x20 and x <= 0x7F:
        return chr(x)
    else:
        return "~"

def build_pattern(target):
    first = ord(target[0])
    chars = []
    i = 0
    while i < len(target):
        c = ord(target[i])
        if not (c >= 65 and c < (65 + 26)):
            raise Exception("Not an uppercase letter: "+char(c))
        chars.append(c - first)
        i += 1
    assert chars[0] == 0
    print("PATTERN:", '"%s"'%target, chars)
    return chars[1:]

def char(c):
    if c >= 0x20 and c <= 0x7F:
        return "'%s'" % chr(c)
    else:
        return "<%d>" % c

def match_pattern(content, offset, pattern):
    first = int(content[offset])
    i = 0
    while i < len(pattern):
        c = int(content[offset + 1 + i])
        matches = (first == c - pattern[i])
        if not matches:
            # look for rot13
            matches = (
                (first == c - pattern[i] + 26)
                or
                (first == c - pattern[i] - 26)
            )
        if not matches:
            return False
        i += 1
    return True

if __name__ == "__main__":
    try:
        main()
    except InvocationException as e:
        print("FILE NOT FOUND: "+filename, file=sys.stderr)
        sys.exit(1)

