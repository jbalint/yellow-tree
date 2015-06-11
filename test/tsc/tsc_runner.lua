-- setup arg table for `tsc' program
arg = {}
table.insert(arg, 1, "-f")
table.insert(arg, 2, "early_return.lua")
table.insert(arg, 3, "array_assignment.lua")

-- run tsc
tsc = loadfile("tsc")
tsc()

-- finish
g()
TelescopeTest.release()
return false
