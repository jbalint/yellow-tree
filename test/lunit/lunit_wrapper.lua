-- ============================================================
-- lunit test framework wrapper/runner
--
-- Currently requires some cooperation from the JVM to not
-- exit before results are reported
-- ============================================================
local lunit = require("lunit")

-- specify tests here
local tests = {}
table.insert(tests, "test_basic.lua")
table.insert(tests, "test_java_bridge_class.lua")
table.insert(tests, "test_breakpoints.lua")

g()
local stats = lunit.main(tests)
if stats.errors > 0 or stats.failed > 0 then
   print("Error")
   os.exit(1)
end
print("ok")
BasicTestClass.notifyRunLock()
