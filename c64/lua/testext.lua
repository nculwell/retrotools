
local function eltPath(path, i, j)
  assert(type(path) == "table")
  assert(type(i) == "number")
  if not j then j = 1 end
  if j > #path then
    return tostring(i)
  else
    return tostring(eltPath[j]) .. "," .. eltPath(path, i, j+1)
  end
end

local function tables_equal_recursive(got, exp, path)
  assert(type(exp) == "table", "Expected value is not a table.")
  assert(type(got) == "table", "Got value is not a table.")
  if #got ~= #exp then
    return false, string.format("Tables have different lengths: expected %d, got %d.", #exp, #got)
  end
  for i = 1, #got do
    if type(exp[i]) == "table" then
      if type(got) ~= "table" then
        return false, string.format("Expected a table at element %s, got %s.", eltPath(path, i), type(got[i]))
      end
      if not tables_equal_recursive(got[i], exp[i]) then
        return false, string.format("Table elements at %s are not equal.", eltPath(path, i))
      end
    else
      if got[i] ~= exp[i] then
        return false, string.format("Elements at %s are not equal: %s ~= %s", eltPath(path, i), tostring(got[i]), tostring(exp[i]))
      end
    end
  end
  return true
end

local function tables_equal(got, exp)
  if not (type(got) == "table") then
    return false, string.format("Expected a table, got %s.", type(got))
  end
  return tables_equal_recursive(got, exp, {})
end

return function(test)
  test.register_assert("tables_equal", tables_equal)
end

