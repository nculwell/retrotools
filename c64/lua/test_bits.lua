local test = require("u-test")
local bits = require("bits")
local bit = require("bit")

function test_bitflag()
  test.is_true(    bits.bitflag(1, bits.parseBin("0b00000001")))
  test.is_true(not bits.bitflag(1, bits.parseBin("0b11111110")))
  test.is_true(    bits.bitflag(2, bits.parseBin("0b00000010")))
  test.is_true(not bits.bitflag(2, bits.parseBin("0b11111101")))
  test.is_true(    bits.bitflag(3, bits.parseBin("0b00000100")))
  test.is_true(not bits.bitflag(3, bits.parseBin("0b11111011")))
  test.is_true(    bits.bitflag(4, bits.parseBin("0b00001000")))
  test.is_true(not bits.bitflag(4, bits.parseBin("0b11110111")))
end

test.bitflag = test_bitflag
test.summary()

