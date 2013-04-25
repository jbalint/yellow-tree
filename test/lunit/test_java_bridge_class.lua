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
   -- TODO set this up
   --assert_equal(str_string, copy_string)

   local success, error = pcall(function () print(java.lang.String.new(1,2,3,4,5)) end)
   if success then
      fail("Constructor call should fail if it doesn't exist")
   end
end
