
local function switch(n)
  label = { FIRST, SECOND, THIRD }[n]
  ::FIRST::
  return 10
  ::SECOND::
  return 20
  ::THIRD::
  return 30
end

print(switch(1))
print(switch(2))
print(switch(3))

