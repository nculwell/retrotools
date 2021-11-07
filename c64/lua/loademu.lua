
local emu = require("emu6502")
local disk = require("c64disk")
local inspect = require("inspect")

function main(args)
  local FileNumber = 2
  local diskFilename = assert(args[1], "No disk filename.")
  local startAddrOverride = args[2]
  local fileData
  if string.match(diskFilename, "%.prg$") then
    local f = assert(io.open(diskFilename, "rb"), "Failed to open file.")
    local fd = f:read("*all")
    --inspect(fd)
    fileData = {}
    for i = 1, string.len(fd) do
      table.insert(fileData, string.byte(fd, i))
    end
    io.close(f)
    assert(#fileData > 2)
  else
    local d = assert(disk.loadDisk(diskFilename))
    local dir = d:readFileDirectory()
    local fileInfo = assert(dir[FileNumber])
    local track = fileInfo.dataTS.track
    local sector = fileInfo.dataTS.sector
    fileData = assert(d:readFile(track, sector))
  end
  --inspect(fileData)
  local emulator = assert(emu.createEmulator())
  local startAddr = emulator:loadPRG(fileData)
  if startAddrOverride then
    emulator:setPC(tonumber(startAddrOverride))
  else
    emulator:setPC(startAddr)
  end
  emulator:interpret()
end

main({ unpack(arg) })

