
local disk = require("c64disk")
local inspect = require("inspect")

local function main(filenames)
  for _, filename in ipairs(filenames) do
    local d = disk.loadDisk(filename)
    local dir = d:readFileDirectory()
    print(filename)
    for _, f in ipairs(dir) do
      print(f:tostring())
    end
    print()
  end
end

main({ unpack(arg) })

