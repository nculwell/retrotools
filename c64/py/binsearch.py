#!/usr/bin/env python3

import os, sys

class InvocationException(Exception):
    pass

def main():
    filename = sys.argv[1]
    target = []
    parse_hex = False
    for x in sys.argv[2:]:
        if x == "-x":
            parse_hex = True
        else:
            if parse_hex:
                target.append(int(x, 16))
            else:
                target.append(int(x, 0))
    if not os.path.exists(filename):
        raise InvocationException("FILE NOT FOUND: "+filename)
    print("FILENAME:", filename)
    print("TARGET:", to_hex(target))
    with open(filename, "rb") as f:
        content = f.read()
    for i in range(0, len(content) - len(target) + 1):
        m = match_pattern(content, i, target)
        if m:
            sequence = [ int(x) for x in content[i:i+len(target)] ]
            print("%06X:" % i, to_hex(sequence))
            pass

def to_hex(sequence):
    return " ".join([ "%02x" % x for x in sequence ])

def match_pattern(content, offset, pattern):
    for i in range(len(pattern)):
        if content[offset + i] != pattern[i]:
            return False
    return True

if __name__ == "__main__":
    try:
        main()
    except InvocationException as e:
        print("FILE NOT FOUND: "+filename, file=sys.stderr)
        sys.exit(1)

