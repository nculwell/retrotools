#!/usr/bin/python3

def encode(text):
    bs = bytearray()
    for i in range(len(text)):
        bs.extend(bytes([ encode_char(text[i]) ]))
    return bytes(bs)

def decode(binary):
    ss = str()
    for i in range(len(binary)):
        ss = ss + decode_char(binary[i])
    return ss

def encode_char(c):
    n = ord(c)
    if ord("@") <= n <= ord("Z"):
        return n - ord("@")
    if not (0x20 <= n < 0x60):
        raise ValueError("Invalid C64 character: " + str(n))
    return n

def decode_char(b):
    n = int(b)
    if n < 0x20:
        return chr(ord("@") + n)
    if n >= 0x40 or n in [0x1C, 0x1E, 0x1F]:
        return "~"
    return chr(n)

TEST_PAIRS = [
    ( "@ABC", bytes([0,1,2,3]) ),
]

def test_encode():
    for (asc, c64) in TEST_PAIRS:
        assert c64 == encode(asc)

def test_decode():
    for (asc, c64) in TEST_PAIRS:
        assert asc == decode(c64)

if __name__ == "__main__":
    import sys
    encoded_hex = "".join(sys.argv[1:]).replace(" ", "")
    if len(encoded_hex) % 2 == 1:
        encoded_hex = "0" + encoded_hex
    encoded_bytes = bytes(( int(encoded_hex[i:i+2], 16) for i in range(0, len(encoded_hex), 2) ))
    print(encoded_bytes)
    print(decode(encoded_bytes))

