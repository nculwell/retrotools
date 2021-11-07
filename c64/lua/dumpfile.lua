local disk = require("c64disk")
local inspect = require("inspect")

local diskImagePath = arg[1]
local fileNumber = arg[2]

assert(diskImagePath and fileNumber)
fileNumber = tonumber(fileNumber)

local d = disk.loadDisk(diskImagePath)
local dir = d:readFileDirectory()
local fileInfo = dir[fileNumber]
inspect(fileInfo)
print(fileInfo:tostring())
local eb = ""
for _, b in ipairs(fileInfo.entryBytes) do
  eb = eb .. string.format(" %02X", b)
end
print(string.sub(eb, 1))
local track, sector = fileInfo["dataTS"]["track"], fileInfo["dataTS"]["sector"]
local length = fileInfo["fileSizeBytes"]
print("TS:", track, sector)
d:dumpFile(track, sector, length)

