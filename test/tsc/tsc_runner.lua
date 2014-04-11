-- setup arg table for `tsc' program
arg = {}
table.insert(arg, 1, "-f")
table.insert(arg, 2, "early_return.lua")

-- run tsc
tsc = loadfile("/home/jbalint/bin/tsc")
tsc()

-- finish
g()
TelescopeTest.release()
return false
