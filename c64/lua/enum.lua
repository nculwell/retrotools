
local ffi = require("ffi")

ffi.cdef[[
enum myconst {
  A = 1, // A
  B = 1 | 2, // B
}
]]

print(ffi.C.A)
print(ffi.C.B)
