#!/usr/bin/python3

import sys

def div8(a, b):
    N1 = b & 0xFF
    N0 = a & 0xFF
    A = (a >> 8) & 0xFF
    Y = 8
    while Y > 0:
        N0 <<= 1
        A <<= 1
        if N0 >= 0x100:
            N0 &= 0xFF
            A |= 1
            carry = True
        else:
            carry = False
        if A >= N1:
            A -= N1
            N0 += 1
        Y -= 1
    return A, N0

if __name__ == "__main__":
    print(div8(*[ int(x) for x in sys.argv[1:] ]))

