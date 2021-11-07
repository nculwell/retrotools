
local disk = require("c64disk")
local inspect = require("inspect")

local function main(args)
  assert(#args == 3, "Wrong number of arguments (expected 3).")
  inspect(args)
  local diskFilename, fileNumberStr, destFilename = unpack(args)
  local fileNumber = tonumber(fileNumberStr)
  local d = disk.loadDisk(diskFilename)
  local dir = d:readFileDirectory()
  local fileInfo = dir[fileNumber]
  inspect(fileInfo)
  print(fileInfo:tostring())
  local track = fileInfo.dataTS.track
  local sector = fileInfo.dataTS.sector
  inspect(track)
  inspect(sector)
  local fileData = d:readFile(track, sector)
  local destFile = io.open(destFilename, "wb")
  if not destFile then
    error("Unable to open destiation file: " .. destFilename)
  end
  for _, b in ipairs(fileData) do
    destFile:write(string.char(b))
  end
  destFile:close()
end

main({ unpack(arg) })

