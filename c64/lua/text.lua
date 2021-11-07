
local acs = require("acs")
local base40 = require("base40")

local text = {}

function text.readShortText(diskImage, shortTextId)
  local offset = acs.SHORTTEXT_OFFSET + int((short_text_id - 1) / 2) * acs.SHORTTEXT_PAIR_LENGTH
  if bits.bitflag(short_text_id, 1) then
    offset = offset + acs.SHORTTEXT_ITEM_LENGTH
  end
  return c64text.decode(diskImage, offset, offset + acs.SHORTTEXT_ITEM_LENGTH)
end

local function trimRight(str)
  local endIndex = string.len(str)
  while string.sub(str, endIndex, endIndex) == " " do
    endIndex = endIndex - 1
  end
  if endIndex < string.len(str) then
    str = string.sub(str, 1, endIndex)
  end
  return str or ""
end

function text.readName(diskImage, namesOffset, nameIndex)
  local offset = namesOffset + nameIndex * acs.NAME_LENGTH
  local nameBytes = { diskImage.read(offset, acs.NAME_LENGTH) }
  local decoded = base40.decode(nameBytes)
  return trimRight(decoded)
end

return text

