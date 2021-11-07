local test = require("u-test")
require("testext")(test)
local c64Text = require("c64text")

local TEST_PAIRS = {
  { "@ABC", { 0, 1, 2, 3 } },
  { " !01/?", { 0x20, 0x21, 0x30, 0x31, 0x2F, 0x3F } },
}

local UNENCODABLE_SYMBOLS = "`{|}~\\^_"

function test.encode()
  for _, t in ipairs(TEST_PAIRS) do
    local encoded = c64Text.encode(t[1])
    test.is_table(encoded)
    test.tables_equal(encoded, t[2])
  end
end

function test.decode()
  for _, t in ipairs(TEST_PAIRS) do
    local decoded = c64Text.decode(t[2])
    test.is_string(decoded)
    test.equal(decoded, t[1])
  end
end

function test.encode_errors()
  for i = 1, string.len(UNENCODABLE_SYMBOLS) do
    local c = string.sub(UNENCODABLE_SYMBOLS, i, i)
    test.error_raised(function() c64Text.encode(c) end)
  end
  for n = 0x60, 0x7F do
    test.error_raised(function() c64Text.encode(string.char(n)) end)
  end
end

test.summary()

