
local ffi = require("ffi")
local bit = require("bit")

local byte = ffi.typeof("uint8_t")
local a = byte(0xFF)

print(tonumber(a), type(a))

a = byte(a + 1)

print(tonumber(a), type(a))

a = byte(0)

print(tonumber(a-1))

print(tonumber(byte(a-1)))

local s = ffi.typeof[[
struct {
  uint8_t x;
  int8_t y;
  int16_t z;
}
]]

local t = s(1,1,1)
print(t.x, t.y, t.z)
t.x = t.x - 1
print(t.x, t.y, t.z)
t.x = t.x - 1
print(t.x, t.y, t.z)
