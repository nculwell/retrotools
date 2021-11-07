-- Commodore 1541 disk drive sectors
-- Source: https://ist.uwaterloo.ca/~schepers/formats/D64.TXT
--
-- Field 1: The track number (count starts from 1)
-- Field 2: How many sectors are in this track
-- Field 3: Offset from the beginning of the disk, in sectors
-- Field 4: Offset, in bytes, of the start byte of this track in a D64 file
--
-- A sector is 256 bytes.

local inspect = require("inspect")
local bits = require("bits")
local c64text = require("c64text")

local module = {}

local SECTOR_FIELD_NAMES = { "Track", "SectorCount", "SectorsOffset", "D64Offset" }

module.SECTOR_SIZE = 0x100

local TRACK_INFO = {

    {  1, 21,   0, 0x00000 },
    {  2, 21,  21, 0x01500 },
    {  3, 21,  42, 0x02A00 },
    {  4, 21,  63, 0x03F00 },
    {  5, 21,  84, 0x05400 },
    {  6, 21, 105, 0x06900 },
    {  7, 21, 126, 0x07E00 },
    {  8, 21, 147, 0x09300 },
    {  9, 21, 168, 0x0A800 },
    { 10, 21, 189, 0x0BD00 },

    { 11, 21, 210, 0x0D200 },
    { 12, 21, 231, 0x0E700 },
    { 13, 21, 252, 0x0FC00 },
    { 14, 21, 273, 0x11100 },
    { 15, 21, 294, 0x12600 },
    { 16, 21, 315, 0x13B00 },
    { 17, 21, 336, 0x15000 },
    { 18, 19, 357, 0x16500 },
    { 19, 19, 376, 0x17800 },
    { 20, 19, 395, 0x18B00 },

    { 21, 19, 414, 0x19E00 },
    { 22, 19, 433, 0x1B100 },
    { 23, 19, 452, 0x1C400 },
    { 24, 19, 471, 0x1D700 },
    { 25, 18, 490, 0x1EA00 },
    { 26, 18, 508, 0x1FC00 },
    { 27, 18, 526, 0x20E00 },
    { 28, 18, 544, 0x22000 },
    { 29, 18, 562, 0x23200 },
    { 30, 18, 580, 0x24400 },

    { 31, 17, 598, 0x25600 },
    { 32, 17, 615, 0x26700 },
    { 33, 17, 632, 0x27800 },
    { 34, 17, 649, 0x28900 },
    { 35, 17, 666, 0x29A00 },
    { 36, 17, 683, 0x2AB00 },
    { 37, 17, 700, 0x2BC00 },
    { 38, 17, 717, 0x2CD00 },
    { 39, 17, 734, 0x2DE00 },
    { 40, 17, 751, 0x2EF00 },

}

local function trackSectorAddress(track, sector)
  local trackLine = TRACK_INFO[track]
  assert(trackLine, "Track not found: " .. tostring(track))
  assert(sector < trackLine[2], "Sector out of range: " .. tostring(sector))
  local trackAddr = trackLine[4]
  assert(trackAddr, "Track address not found: " .. tostring(track))
  return trackAddr + sector * 0x100
end

function module.getTrackInfo()
  local si = {}
  for _, track in ipairs(TRACK_INFO) do
    local tn, sit, ss, off = unpack(track)
    table.insert(si, {
      trackNumber = tn,
      sectorsInTrack = sit,
      startSector = ss,
      offset = off,
    })
  end
  return si
end

local FILE_TYPES = {
  [0] = "DEL",
  [1] = "SEQ",
  [2] = "PRG",
  [3] = "USR",
  [4] = "REL",
}

local function decodeFileName(bytes)
  local len = #bytes
  while len > 0 and bytes[len] == 0xA0 do
    len = len - 1
  end
  local name = ""
  for i = 1, len do
    local c = bytes[i]
    if c < 0x20 or c >= 0x7F then c = string.byte("~") end
    name = name .. string.char(c)
  end
  return name
end

local function diskDirectoryToString(self)
  local flags = ""
  if self.isLocked then flags = flags .. ">" end
  if not self.isClosed then flags = flags .. "*" end
  if string.len(flags) > 0 then flags = " [" .. flags .. "]" end
  local str = string.format(
  "%s |%s|%s at %d, %d ($%06X), %d sectors ($%X)",
  self.fileType, self.name, flags, self.dataTS.track, self.dataTS.sector, self.dataOffset, self.fileSizeSectors, self.fileSizeSectors * module.SECTOR_SIZE)
  if self.loadAddr then
    str = str .. string.format(", load at $%04X", self.loadAddr)
  end
  return str
end

function module.readFileDirectory(d64, printDirectory)
  local function ts(t, s) return { track=t, sector=s } end
  local trkInf = module.getTrackInfo()
  --print(inspect(trkInf))
  local TRACK_18_OFFSET = trkInf[18].offset
  local SECTOR_SIZE = module.SECTOR_SIZE
  local DIRECTORY_ENTRY_SIZE = 0x20
  local SECTOR_1_OFFSET = TRACK_18_OFFSET + SECTOR_SIZE
  -- There's a track/sector reference here on the disk but it's ignored and
  -- 18,1 is always used.
  local nextTrack, nextSector = 18, 1
  local directoryEntries = {}
  while true do
    secOff = trkInf[nextTrack].offset + nextSector * SECTOR_SIZE
    nextTrack, nextSector = d64.read(secOff, 2)
    for off = secOff, secOff + SECTOR_SIZE - 1, DIRECTORY_ENTRY_SIZE do
      local fileTrack, fileSector = d64.read(off+3, 2)
      if fileTrack > 0 then
        --print(string.format("FILE: offset %05X, track %d, sector %d", off, fileTrack, fileSector))
        --inspect({ d64.read(off, 5) }, { hex=true })
        if fileTrack > #trkInf then
          -- Aborting here because this probably indicates a bug or an invalid
          -- disk directory.
          error(string.format("Track number out of range: %d", fileTrack))
        end
        local fileEntryBytes = { d64.read(off, 0x20) }
        local fileTypeByte = d64.read(off+2)
        local fileTypeId = bits.mask3BitLo(fileTypeByte)
        local fileTypeName = FILE_TYPES[fileTypeId]
        if not fileTypeName then
          error(string.format("Invalid file type: %X", fileTypeId))
        end
        local fileLocked = bits.bitflag(7, fileTypeByte)
        local fileClosed = bits.bitflag(8, fileTypeByte)
        local fileName = { d64.read(off+5, 0x10) }
        local fileSizeLo, fileSizeHi = d64.read(off+0x1E, 2)
        local dataOffset = trackSectorAddress(fileTrack, fileSector)
        local loadAddr
        local dirEntry = {
          tostring = diskDirectoryToString,
          fileType = fileTypeName,
          isLocked = fileLocked or nil,
          isClosed = fileClosed,
          name = decodeFileName(fileName),
          dataTS = ts(fileTrack, fileSector),
          fileSizeSectors = bits.word(fileSizeLo, fileSizeHi),
          dataOffset = dataOffset,
          entryBytes = fileEntryBytes,
        }
        dirEntry.fileSizeBytes = dirEntry.fileSizeSectors * 256
        if fileTypeName == "PRG" then
          dirEntry.loadAddr = bits.word(d64.read(dataOffset+2, 2))
        end
        if fileTypeName == "REL" then
          local sideTrack, sideSector = d64.read(off+0x15, 2)
          local relFileRecLen = d64.read(off+0x17)
          dirEntry.relSideSectorBlockTS = ts(sideTrack, sideSector)
          dirEntry.relFileRecLen = relFileRecLen
        end
        table.insert(directoryEntries, dirEntry)
      end
    end
    if nextTrack == 0 then
      break
    end
  end
  return directoryEntries
end

local function readFile(disk, track, sector)
  local blockAddr = trackSectorAddress(track, sector)
  local nextBlockTrack, nextBlockSector = disk.read(blockAddr, 2)
  local fileData = { disk.read(blockAddr+2, 0x100-2) }
  while nextBlockTrack > 0 do
    blockAddr = trackSectorAddress(nextBlockTrack, nextBlockSector)
    nextBlockTrack, nextBlockSector = disk.read(blockAddr, 2)
    for i = 1, (0x100-2) do
      local addr = blockAddr+2+i-1
      local byte = disk.read(addr)
      table.insert(fileData, byte)
    end
  end
  return fileData
end

local function dumpFile(disk, track, sector)
  assert(disk and track and sector)
  local nextBlockTrack, nextBlockSector = track, sector
  --local blockAddr = trackSectorAddress(track, sector)
  --local nextBlockTrack, nextBlockSector = disk.read(blockAddr, 2)
  --local fileData = { disk.read(blockAddr+2, 0x100-2) }
  print(string.format("File at track:sector %d:%d", track, sector))
  while nextBlockTrack > 0 do
    local blockAddr = trackSectorAddress(nextBlockTrack, nextBlockSector)
    print(string.format("TRACK: %d", nextBlockTrack))
    print(string.format("SECTOR: %d", nextBlockSector))
    print(string.format("ADDR: %02X", blockAddr))
    nextBlockTrack, nextBlockSector = disk.read(blockAddr, 2)
    if nextBlockTrack == 0 then
      print(string.format("LAST: $%02X bytes", nextBlockSector))
    else
      print(string.format("NEXT: %d:%d", nextBlockTrack, nextBlockSector))
    end
    local line = ""
    local count = 0
    for i = 1, (0x100-2) do
      count = count + 1
      local addr = blockAddr+2+i-1
      local byte = disk.read(addr)
      line = line .. string.format(" %02X", byte)
      if i % 16 == 0 then
        print(string.sub(line, 2))
        line = ""
      end
      if nextBlockTrack == 0 then
        if count == nextBlockSector then
          break
        end
      end
    end
    if string.len(line) > 0 then
      print(string.sub(line, 2))
    end
  end
  return fileData
end

local function randomAccessReader(data)
  return function(offset, length)
    if not length then length = 1 end
    assert(type(offset) == "number", "Read offset is not a number.")
    assert(type(length) == "number", "Read length is not a number.")
    if offset + length > string.len(data) then
      error(string.format("Attempt to read beyond end of file: %X > %X.", offset + length, string.len(data)))
    end
    local values = {}
    for i = 1, length do
      local character = string.sub(data, offset + i, offset + i)
      local byte = string.byte(character)
      table.insert(values, byte)
    end
    return unpack(values)
  end
end

function module.loadDisk(path)
  if not path then
    error("No disk image path specified.")
  end
  local f = io.open(path, "rb")
  if not f then
    error("Unable to open disk image file: " .. path)
  end
  local fileData = f:read("*a")
  f:close()
  if not fileData then
    error("Unable to read disk image file: " .. path)
  end
  return {
    read = randomAccessReader(fileData),
    length = string.len(fileData),
    readFileDirectory = module.readFileDirectory,
    readFile = readFile,
    dumpFile = dumpFile,
  }
end

return module

