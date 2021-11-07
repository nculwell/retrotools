-- Functions to encode/decode the C64 character encoding (to/from ASCII).

local c64Text = {}

local function strImplode(bytes)
  for i = 1, #bytes do
  end
end

local function strExplode(str)
end

function c64Text.encode(asciiText)
  local c64Bytes = {}
  for i = 1, string.len(asciiText)  do
    c64Bytes[i] = c64Text.encodeChar(string.sub(asciiText, i, i))
  end
  return c64Bytes
end

function c64Text.decode(c64Bytes, beginIndex, endIndex)
  if beginIndex == nil then beginIndex = 1 end
  if endIndex == nil then endIndex = #c64Bytes end
  asciiText = ""
  for i = beginIndex, endIndex do
    asciiText = asciiText .. c64Text.decodeChar(c64Bytes[i])
  end
  return asciiText
end

function c64Text.encodeChar(asciiChar)
  assert(type(asciiChar) == "string")
  --print("ASCII CHAR: '" .. asciiChar .. "'")
  local n = string.byte(asciiChar)
  --assert(n, "Oops, n is " .. tostring(n))
  if 0x40 <= n and n <= 0x5A or n == 0x5B or n == 0x5D then
    return n - 0x40
  end
  if 0x20 <= n and n < 0x40 then
    return n
  end
  local repr = asciiChar
  if n < 0x20 or n >= 0x7F then
    repr = "<" .. tostring(n) .. ">"
  end
  error("ASCII character cannot be encoded as C64: " .. repr)
end

function c64Text.decodeChar(n)
  assert(type(n) == "number")
  assert(0 <= n and n <= 255)
  if n < 0x20 and n ~= 0x1C and n ~= 0x1E and n ~= 0x1F then
    return string.char(0x40 + n)
  end
  if n < 0x40 then
    return string.char(n)
  end
  return "~"
end

return c64Text

