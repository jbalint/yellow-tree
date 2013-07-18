-- ============================================================
-- Tests for breakpoints
-- ============================================================
require("lunit")
module("test_breakpoints", lunit.testcase, package.seeall)

-- This is a bit of a weird case because of how the breakpoint is hit on the same thread as the calling code
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
   bc()
end

function test_breakpoint_on_main_thread()
   local x = 1
   bp("BasicTestClass.doSomething()V")
   bl()[1].handler = function (bp, thread)
	  x = 2
	  return false
   end
   -- dont let debugger handle the breakpoint before we wait
   debug_lock:lock()
   BasicTestClass.notifyRunLock()
   debug_event:lock()
   debug_lock:unlock()
   debug_event:wait()
   debug_event:unlock()
   g()
   assert_equal(2, x)
   bc()
end

function test_breakpoint_setting_by_method_id()
end
