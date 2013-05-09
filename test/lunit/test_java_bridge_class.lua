-- ============================================================
-- Tests for java_bridge
-- ============================================================
require("lunit")
module("test_java_bridge_class", lunit.testcase, package.seeall)

function test_basic_class_access()
   local string_class = java.lang.String
   assert_equal(lj_find_class("java/lang/String"), string_class, "Class objects should be equal when accessed differently")
   assert_equal("java.lang.String", string_class.name)
   assert_string(string_class.name)
   assert_equal("String.java", string_class.sourcefile)
end

function test_basic_instantiation()
   local empty_string = java.lang.String.new()
   assert_equal(0, empty_string.length())
   assert_equal(java.lang.String, empty_string.class)
   local num_string = java.lang.String.new(4000)
   assert_equal("4000", num_string.toString())
   local bool_string = java.lang.String.new(false)
   assert_equal("false", bool_string.toString())
   local str_string = java.lang.String.new("Hello")
   assert_equal("Hello", str_string.toString())
   local copy_string = java.lang.String.new(str_string)
   assert_equal("Hello", copy_string.toString())
   assert_equal(str_string, copy_string)

   -- ensure ctor is properly called
   local test_o = BasicTestClass.new("the_val")
   assert_not_nil(test_o)
   assert_equal("the_val", test_o.myVal.toString())

   local success, error = pcall(function () print(java.lang.String.new(1,2,3,4,5)) end)
   if success then
      fail("Constructor call should fail if it doesn't exist")
   end
end

function test_basic_field_access()
   -- static field
   local string_class = java.lang.String
   local cio_field = string_class.fields.CASE_INSENSITIVE_ORDER
   local cio = string_class.CASE_INSENSITIVE_ORDER
   assert_not_nil(cio_field)
   assert_not_nil(cio)
   assert_true(cio_field.modifiers.static)
   assert_equal(java.lang["String$CaseInsensitiveComparator"], cio.class)

   -- instance field
   local test_o = BasicTestClass.new("the_val")
   local myVal_field = BasicTestClass.fields.myVal
   assert_not_nil(myVal_field)
   assert_true(myVal_field.modifiers.private)
   assert_equal("Ljava/lang/String;", myVal_field.sig)
   assert_equal("the_val", test_o.myVal.toString())

   -- equality
   local f1 = java.lang.String.fields.value
   local f2 = java.lang.String.fields.value
   assert_not_nil(f1)
   assert_equal(f1, f2)
   local f3 = java.lang.String.fields.hash
   assert_not_nil(f3)
   assert_not_equal(f3, f2)
end

function test_basic_method_access()
   -- static method
   local thread_class = java.lang.Thread
   local currentThread_method = thread_class.methods["currentThread()Ljava/lang/Thread;"]
   local currentThread = thread_class.currentThread()
   assert_not_nil(currentThread_method)
   assert_not_nil(currentThread)
   assert_true(currentThread_method.modifiers.static)
   assert_equal(java.lang.Thread, currentThread.class)

   -- instance method
   local getName_method = thread_class.methods["getName()Ljava/lang/String;"]
   assert_not_nil(getName_method)
   assert_false(getName_method.modifiers.static)
   assert_true(getName_method.modifiers.public)
   currentThread.setName("lunit_test!")
   assert_equal("lunit_test!", currentThread.getName().toString())

   -- equality
   local m1 = java.lang.Thread.methods["getName()Ljava/lang/String;"]
   local m2 = java.lang.Thread.methods["getName()Ljava/lang/String;"]
   local m3 = java.lang.Thread.methods["setName(Ljava/lang/String;)V"]
   assert_not_nil(m1)
   assert_equal(m1, m2)
   assert_not_nil(m3)
   assert_not_equal(m1, m3)
end

function test_boolean_param()
   local s_true = java.lang.String.valueOf(true)
   assert_equal("true", s_true.toString())
end

-- ============================================================
-- Array access
-- ============================================================
function test_method_invocation_array_matching()
   local tt = TestTypes.new()
   tt.assign()

   local ts = java.util.Arrays.toString

   assert_equal("[null, null, number2, null, null]", ts(tt.oarray))
   assert_equal("[false, false, false, true, false]", ts(tt.zarray))
   assert_equal("[0, 0, 0, 0, -100]", ts(tt.barray))
   assert_equal("[A,  ,  ,  ,  ]", ts(tt.carray))
   assert_equal("[0, 85, 0, 0, 0]", ts(tt.sarray))
   assert_equal("[0, 0, 850, 0, 0]", ts(tt.iarray))
   assert_equal("[0, 0, 0, 8500, 0]", ts(tt.jarray))
   assert_equal("[0.0, 50.1, 0.0, 0.0, 0.0]", ts(tt.farray))
   assert_equal("[0.0, 0.0, 40.12345, 0.0, 0.0]", ts(tt.darray))
end

function test_array_string_stuff()
   local s = java.lang.String.new("Hello!")
   local ba = s.getBytes()
   local ca = s.toCharArray()
   assert_equal(6, ba.length)
   assert_equal(6, ca.length)
   local s2b = java.lang.String.new(ba)
   local s2c = java.lang.String.new(ca)
   assert_equal("Hello!", s2b.toString())
   assert_equal("Hello!", s2c.toString())
end

function test_array_call()
   local tt1 = TestTypes.new()
   tt1.assign()
   local tt2 = TestTypes.new()
   assert_nil(tt2.oarray[3])
   tt2.setArray(tt1.oarray)
   assert_equal("number2", tt2.oarray[3].toString())
end

function test_array_access()
   local tt = TestTypes.new()
   tt.assign()
   assert_equal(5, tt.oarray.length)

   assert_equal(nil, tt.oarray[2])
   assert_equal("number2", tt.oarray[3].toString())

   assert_equal(false, tt.zarray[3])
   assert_equal(true, tt.zarray[4])

   assert_equal(0, tt.barray[1])
   assert_equal(-100, tt.barray[5])

   assert_equal(0x20, tt.carray[2])
   assert_equal(0x41, tt.carray[1])

   assert_equal(0, tt.sarray[1])
   assert_equal(85, tt.sarray[2])

   assert_equal(0, tt.iarray[1])
   assert_equal(850, tt.iarray[3])

   assert_equal(0, tt.jarray[1])
   assert_equal(8500, tt.jarray[4])

   assert_equal(0, tt.farray[1])
   assert_true(tt.farray[2] > 50.0)
   assert_true(math.abs(50.1 - tt.farray[2]) < 0.01)

   assert_equal(0, tt.darray[1])
   assert_true(tt.darray[3] > 40.1234)
   assert_true(math.abs(40.12345 - tt.darray[3]) < 0.0001)
   assert_equal(40.12345, tt.darray[3])
end
