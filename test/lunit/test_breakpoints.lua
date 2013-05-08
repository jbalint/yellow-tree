-- ============================================================
-- Tests for breakpoints
-- ============================================================
require("lunit")
module("test_breakpoints", lunit.testcase, package.seeall)

function test_breakpoint_setting_by_string()
   bp("java/lang/StringBuilder.append(Z)Ljava/lang/StringBuilder;")
   bl()
   bl()[1].handler = function () print("Ok, the bp has been hit!") return false end
   g()
   assert_equal(1, #bl())
   java.lang.StringBuilder.new().append(true)
   print("OK!")
end

function test_breakpoint_setting_by_method_id()
end
