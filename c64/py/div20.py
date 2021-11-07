#!/usr/bin/python3

# Implement ROH algorithm for division by 20.
# Shows results for all values of A.

def add(A):
    Y = 0
    while True:
        A = (A + 0xEC) & 0XFF
        if A >= 0xEC:
            break
        Y += 1
    return (A + 0x14) & 0xFF, Y

for i in range(0x100):
    print(add(i))

