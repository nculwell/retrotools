-- Bitwise operations

local bits = {}

function bits.parseBin(binStr)
  local n = 0
  local shift = #binStr - 1
  local start = 1
  if string.sub(binStr, 1, 2) == "0b" then
    start = 3
    shift = shift - 2
  end
  for i = start, #binStr do
    local c = string.sub(binStr, i, i)
    local b
    if c == "0" then
      b = 0
    elseif c == "1" then
      b = 1
    elseif c == " " then
      -- ignore
    else
      error("Invalid character in binary string: " .. c)
    end
    if b == 1 then
      n = bit.bor(n, bit.lshift(1, shift))
    end
    shift = shift - 1
  end
  return n
end

function bits.mask7Bit(b) return bit.band(b, 0x7F) end
function bits.mask6Bit(b) return bit.band(b, 0x3F) end
function bits.mask5Bit(b) return bit.band(b, 0x1F) end
function bits.mask4BitLo(b) return bit.band(b, 0x0F) end
function bits.mask4BitHi(b) return bit.rshift(bit.band(b, 0xF0), 4) end
function bits.mask3BitLo(b) return bit.band(b, 0x07) end
function bits.mask2BitLo(b) return bit.band(b, 0x03) end

function bits.word(lo, hi) return bit.bor(lo, bit.lshift(hi, 8)) end

function bits.bitflag(bitnumber, b)
  assert(1 <= bitnumber and bitnumber <= 8, "Bit number out of range (1-8): " .. tonumber(bitnumber))
  assert(b, "Byte argument is invalid: " .. tostring(b))
  local shift = bitnumber - 1
  local shifted = bit.rshift(b, shift)
  local masked = bit.band(shifted, 1)
  return 1 == masked
end

return bits

