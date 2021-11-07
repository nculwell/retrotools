-- Functions for converting text to/from base 40 encoding.

local base40 = {}

local function isDivisible(n, divisor)
  local _, frac = math.modf(n, divisor)
  return ((frac or 0) == 0)
end

local function isEven(n)
  local _, frac = math.modf(n, 2)
  return ((frac or 0) == 0)
end

function base40.decode(base40Bytes)
  assert(type(base40Bytes) == "table")
  assert(isEven(#base40Bytes), "Base 40 string must have an even number of bytes.")
  local decoded = ""
  for i = 1, #base40Bytes, 2 do
    local loByte = base40Bytes[i]
    local hiByte = base40Bytes[i+1]
    --print(i, loByte, hiByte)
    local word = bit.bor(bit.lshift(hiByte, 8), loByte)
    local b3 = math.fmod(word, 40)
    local leftover = math.floor(word / 40)
    local b2 = math.fmod(leftover, 40)
    local b1 = math.floor(leftover / 40)
    local bytes = { b1, b2, b3 }
    --print(word, unpack(bytes))
    for j = 1, 3 do
      decoded = decoded .. base40.decodeChar(bytes[j])
    end
  end
  --print(string.format("DECODED: '%s'", decoded))
  return decoded
end

function base40.encode(asciiText)
  assert(type(asciiText) == "string")
  assert(isDivisible(#asciiText, 3), "String length must be divisible by 3 to encode as base 40.")
  local encoded = {}
  for i = 1, #asciiText, 3 do
    --print(string.sub(asciiText, i, i+2))
    local word = 0
    local c1 = string.sub(asciiText, i, i)
    local c2 = string.sub(asciiText, i+1, i+1)
    local c3 = string.sub(asciiText, i+2, i+2)
    word = word + (base40.encodeChar(c1) * 1600)
    word = word + (base40.encodeChar(c2) * 40)
    word = word + (base40.encodeChar(c3))
    local loByte = bit.band(word, 0xFF)
    local hiByte = bit.band(bit.rshift(word, 8), 0xFF)
    table.insert(encoded, loByte)
    table.insert(encoded, hiByte)
  end
  --print(unpack(encoded))
  --print(base40.decode(encoded))
  return encoded
end

function base40.encodeChar(c)
  local n = string.byte(c)
  if n == 0 or n == 0x20 then
    return 0
  elseif string.byte("A") <= n and n <= string.byte("Z") then
    return n - string.byte("A") + 1
  elseif string.byte("1") <= n and n <= string.byte("9") then
    return n - string.byte("1") + 0x1B
  elseif c == "0" then
    return string.byte("O")
  elseif c == "." then
    return 0x24
  elseif c == "," then
    return 0x25
  elseif c == "-" then
    return 0x26
  elseif c == "'" then
    return 0x27
  end
  error("Invalid char to encode: " .. tostring(n))
end

function base40.decodeChar(n)
  assert(n < 256, "Character out of range: " .. tostring(n))
  if n == 0 then
    return " "
  elseif n <= 26 then
    return string.char(string.byte("A")+(n-1))
  elseif 0x1B <= n and n <= 0x1B + 9 - 1 then
    return string.char(string.byte("1")+(n-0x1B))
  elseif n == 0x24 then
    return "."
  elseif n == 0x25 then
    return ","
  elseif n == 0x26 then
    return "-"
  elseif n == 0x27 then
    return "'"
  end
  error("Invalid char value to decode: " .. tostring(n))
end

return base40

