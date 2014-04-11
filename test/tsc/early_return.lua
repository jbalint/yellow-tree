describe("Frame:force_return()", function ()
 -- This test works with EarlyReturnTest to ensure that the assignment
 -- to EarlyReturnTest.value is skipped by forcing early return from
 -- the method that returns the new value to be assigned
 context("early return string", function ()
 it("should override the hard-coded return value", function ()
	   local testObj = EarlyReturnTest.new()
	   assert_equal("initial value", testObj.value.toString())
	   bp("EarlyReturnTest.setValue()Ljava/lang/String;").handler = function (bp, thread)
		  debug_thread.frames[1]:force_return("overridden value")
																	end
	   testObj.test()
	   assert_equal("overridden value", testObj.value.toString())
 end)
 end)

 -- This test is very similar to the previous test. It calls `test()'
 -- on EarlyReturnTest but returns before any assignment to `value' is
 -- done
 context("early return void", function ()
 it("should skip assignment completely", function ()
	   local testObj = EarlyReturnTest.new()
	   assert_equal("initial value", testObj.value.toString())
	   bp("EarlyReturnTest.test()V").handler = function (bp, thread)
		  debug_thread.frames[1]:force_return()
											   end
	   testObj.test()
	   assert_equal("initial value", testObj.value.toString())
 end)
 end)

end)
