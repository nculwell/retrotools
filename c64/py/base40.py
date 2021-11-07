#!/usr/bin/python3

def base40_decode(base40_bytes):
    if len(base40_bytes) % 2 != 0:
        raise Exception("Base 40 string must have an even number of bytes.")
    decoded_chars = []
    for i in range(0, len(base40_bytes), 2):
        lo_byte = base40_bytes[i]
        hi_byte = base40_bytes[i+1]
        word = (hi_byte << 8) | lo_byte
        b1 = int(word / 1600) % 40
        b2 = int(word / 40) % 40
        b3 = word % 40
        decoded_chars += [ decode_char(b) for b in [b1, b2, b3] ]
    return "".join(decoded_chars)

def base40_encode(to_encode):
    if len(to_encode) % 3 != 0:
        raise Exception("String length must be divisible by 3 to encode as base 40.")
    encoded_bytes = bytes()
    for i in range(0, len(to_encode), 3):
        word = 0
        word += encode_char(to_encode[i]) * 1600
        word += encode_char(to_encode[i+1]) * 40
        word += encode_char(to_encode[i+2])
        lo_byte = word & 0xFF
        hi_byte = (word >> 8) & 0xFF
        encoded_bytes += bytes([ lo_byte, hi_byte ])
    return encoded_bytes

def encode_char(c):
    n = ord(c)
    if n == 0 or n == 0x20:
        return 0
    if ord("A") <= n <= ord("Z"):
        return n - ord("A") + 1
    if ord("1") <= n <= ord("9"):
        return n - ord("1") + 0x1B
    if c == "0":
        return ord("O")
    if c == ".":
        return 0x24
    if c == ",":
        return 0x25
    if c == "-":
        return 0x26
    if c == "'":
        return 0x27
    raise Exception("Invalid char to encode: %d" % n)

def decode_char(n):
    if n == 0:
        return " "
    if n <= 26:
        return chr(ord("A")+(n-1))
    if 0x1B <= n <= 0x1B + 9 - 1:
        return chr(ord("1")+(n-0x1B))
    if n == 0x24: return "."
    if n == 0x25: return ","
    if n == 0x26: return "-"
    if n == 0x27: return "'"
    raise Exception("Invalid char value to decode: %d" % n)

CROCODILE_ENCODED = [ 0x9F, 0x15, 0x1C, 0x15, 0x25, 0x3A, 0, 0, 0, 0 ]
CROCODILE_DECODED = "CROCODILE      "

TEST_CASES_A = [
    ("A  ", [ 0x40, 0x06 ]),
    (" A ", [ 0x28, 0x00 ]),
    ("  A", [ 0x01, 0x00 ]),
]

TEST_CASES = [
    ("RIVER VALLEY", [ 0xFE, 0x71, 0x10, 0x22, 0xB4, 0x89, 0xE1, 0x4B ]),
]

def test_encode_crocodile():
    enc = base40_encode(CROCODILE_DECODED)
    assert enc == bytes(CROCODILE_ENCODED)

def test_decode_crocodile():
    dec = base40_decode(bytes(CROCODILE_ENCODED))
    assert dec == CROCODILE_DECODED

def run_test_cases_encode(test_cases):
    for (a, b) in test_cases:
        assert base40_encode(a) == bytes(b)

def run_test_cases_decode(test_cases):
    for (a, b) in test_cases:
        assert a == base40_decode(bytes(b))

def test_encode_a():
    run_test_cases_encode(TEST_CASES_A)

def test_decode_a():
    run_test_cases_decode(TEST_CASES_A)

def test_encode():
    run_test_cases_encode(TEST_CASES)

def test_decode():
    run_test_cases_decode(TEST_CASES)

def test_encode_chars():
    assert  0 == encode_char(" ")
    assert  1 == encode_char("A")
    assert  2 == encode_char("B")
    assert  3 == encode_char("C")
    assert 26 == encode_char("Z")
    assert 27 == encode_char("1")
    assert 31 == encode_char("5")
    assert 35 == encode_char("9")
    assert 36 == encode_char(".")
    assert 37 == encode_char(",")
    assert 38 == encode_char("-")
    assert 39 == encode_char("'")

def test_decode_chars():
    assert " " == decode_char( 0)
    assert "A" == decode_char( 1)
    assert "B" == decode_char( 2)
    assert "C" == decode_char( 3)
    assert "Z" == decode_char(26)
    assert "1" == decode_char(27)
    assert "5" == decode_char(31)
    assert "9" == decode_char(35)
    assert "." == decode_char(36)
    assert "," == decode_char(37)
    assert "-" == decode_char(38)
    assert "'" == decode_char(39)

def hex_to_bytes(hex_string):
    if len(hex_string) % 2 == 1:
        hex_string = "0" + hex_string
    hex_bytes = bytes(( int(hex_string[i:i+2], 16) for i in range(0, len(hex_string), 2) ))
    return hex_bytes

if __name__ == "__main__":
    import sys
    if sys.argv[1] == "encode":
        _, cmd, arg = sys.argv
        encoded = base40_encode(arg)
        for b in encoded:
            print("%02X " % b, end="")
        print()
    else:
        encoded_hex = "".join(sys.argv[1:]).replace(" ", "")
        encoded_bytes = hex_to_bytes(encoded_hex)
        print(encoded_bytes)
        print(base40_decode(encoded_bytes))

