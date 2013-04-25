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
