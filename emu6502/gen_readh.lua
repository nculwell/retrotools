
return function(path)
  local f = assert(io.open(path, "r"), "Unable to open header file: "..path)
  local inPreprocessor = false
  local lines = {}
  for line in f:lines() do
    if inPreprocessor then
      if not string.match(line, "%\\ *$") then
        inPreprocessor = false
      end
    elseif string.match(line, "^ *#") then
      inPreprocessor = true
    else
      table.insert(lines, line)
    end
  end
  f:close()
  return table.concat(lines, "\n")
end

