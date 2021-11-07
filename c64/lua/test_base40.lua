local test = require("u-test")
require("testext")(test)
local base40 = require("base40")

local CROCODILE_ENCODED = { 0x9F, 0x15, 0x1C, 0x15, 0x25, 0x3A, 0, 0, 0, 0 }
local CROCODILE_DECODED = "CROCODILE      "

local TEST_CASES_A = {
  { "A  ", { 0x40, 0x06 } },
  { " A ", { 0x28, 0x00 } },
  { "  A", { 0x01, 0x00 } },
}

local TEST_CASES = {
  { "RIVER VALLEY", { 0xFE, 0x71, 0x10, 0x22, 0xB4, 0x89, 0xE1, 0x4B } },
}

function test.encode_crocodile()
  local enc = base40.encode(CROCODILE_DECODED)
  --print(unpack(enc))
  test.tables_equal(enc, CROCODILE_ENCODED)
end

function test.decode_crocodile()
  local dec = base40.decode(CROCODILE_ENCODED)
  test.equal(dec, CROCODILE_DECODED)
end

function run_test_cases_encode(test_cases)
  for _, tc in ipairs(test_cases) do
    local a, b = unpack(tc)
    test.tables_equal(b, base40.encode(a))
  end
end

function run_test_cases_decode(test_cases)
  for _, tc in ipairs(test_cases) do
    local a, b = unpack(tc)
    test.equal(base40.decode(b), a)
  end
end

function test.encode_a()
  run_test_cases_encode(TEST_CASES_A)
end

function test.decode_a()
  run_test_cases_decode(TEST_CASES_A)
end

function test.encode()
  run_test_cases_encode(TEST_CASES)
end

function test.decode()
  run_test_cases_decode(TEST_CASES)
end

function test.encodeChars()
  test.equal( 0, base40.encodeChar(" "))
  test.equal( 1, base40.encodeChar("A"))
  test.equal( 2, base40.encodeChar("B"))
  test.equal( 3, base40.encodeChar("C"))
  test.equal(26, base40.encodeChar("Z"))
  test.equal(27, base40.encodeChar("1"))
  test.equal(31, base40.encodeChar("5"))
  test.equal(35, base40.encodeChar("9"))
  test.equal(36, base40.encodeChar("."))
  test.equal(37, base40.encodeChar(","))
  test.equal(38, base40.encodeChar("-"))
  test.equal(39, base40.encodeChar("'"))
end

function test.decodeChars()
  test.equal(" ", base40.decodeChar( 0))
  test.equal("A", base40.decodeChar( 1))
  test.equal("B", base40.decodeChar( 2))
  test.equal("C", base40.decodeChar( 3))
  test.equal("Z", base40.decodeChar(26))
  test.equal("1", base40.decodeChar(27))
  test.equal("5", base40.decodeChar(31))
  test.equal("9", base40.decodeChar(35))
  test.equal(".", base40.decodeChar(36))
  test.equal(",", base40.decodeChar(37))
  test.equal("-", base40.decodeChar(38))
  test.equal("'", base40.decodeChar(39))
end

