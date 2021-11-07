#!/usr/bin/python3

import sys, subprocess

def word(s):
    n = int(s, 16) & 0xFFFF
    return bytes([ n & 0xFF, (n>>8) & 0xFF ])

def byte(s):
    n = int(s, 16) & 0xFF
    return bytes([ n ])

def binByte(s):
    n = int(s, 2)
    assert(n >= 0 and n < 0x100)
    return bytes([ n ])

_, stateFilePath = sys.argv

with open(stateFilePath) as f:
    lines = [ ln.strip().split('=', maxsplit=1) for ln in f.readlines() ]
props = { k:v for k, v in lines }

print(props)
print(hex(int(props["PC"], 16)))
regFilePath = stateFilePath + ".reg"
with open(regFilePath, "wb") as f:
    f.write(word(props["PC"]))
    f.write(byte(props["A"]))
    f.write(byte(props["X"]))
    f.write(byte(props["Y"]))
    f.write(byte(props["SP"]))
    f.write(binByte(props["P"]))

cmd = ["./c64emulator", "state", regFilePath, props["RAM"], props["DISK"]]
subprocess.run(cmd, check=True)

