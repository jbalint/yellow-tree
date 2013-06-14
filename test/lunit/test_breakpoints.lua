-- ============================================================
-- Tests for breakpoints
-- ============================================================
require("lunit")
module("test_breakpoints", lunit.testcase, package.seeall)

-- This is a bit of a weird case because of how the
function test_breakpoint_setting_by_string()
   local x = 1
   bp("java/lang/StringBuilder.append(Z)Ljava/lang/StringBuilder;")
   assert_equal(1, #bl())
   bl()[1].handler = function ()
      x = 2
      return true
   end
   java.lang.StringBuilder.new().append(true)
   assert_equal(2, x)
end

function test_breakpoint_setting_by_method_id()
end
