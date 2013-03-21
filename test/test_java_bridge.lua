 --       _                  ____       _     _              _______        _       
 --      | |                |  _ \     (_)   | |            |__   __|      | |      
 --      | | __ ___   ____ _| |_) |_ __ _  __| | __ _  ___     | | ___  ___| |_ ___ 
 --  _   | |/ _` \ \ / / _` |  _ <| '__| |/ _` |/ _` |/ _ \    | |/ _ \/ __| __/ __|
 -- | |__| | (_| |\ V / (_| | |_) | |  | | (_| | (_| |  __/    | |  __/\__ \ |_\__ \
 --  \____/ \__,_| \_/ \__,_|____/|_|  |_|\__,_|\__, |\___|    |_|\___||___/\__|___/
 --                                              __/ |                              
 --                                             |___/                               

print("*****************************")
print("Running java_bridge tests....")

function test_method_id_comparison()
   io.write("Running test_method_id_comparison().....")

   local m1 = lj_get_method_id("java/lang/String", "toUpperCase", "", "Ljava/lang/String;")
   local m2 = lj_get_method_id("java/lang/String", "toUpperCase", "", "Ljava/lang/String;")
   assert(m1 and m2)
   -- should compare as equal
   assert(m1 == m2)

   local m3 = lj_get_method_id("java/lang/String", "toString", "", "Ljava/lang/String;")
   local m4 = lj_get_method_id("java/lang/Class", "toString", "", "Ljava/lang/String;")
   assert(m3 and m4)
   -- different classes means different methods
   assert(m3 ~= m4)
   assert(m1 ~= m3)

   local m5 = lj_get_method_id("blabbity bla", "asd", "", "V")
   assert(m5 == nil)

   print("ok")
end

-- not comprehensive
function test_basic_field_access()
   io.write("Running test_basic_field_access().....")

   local t = lj_get_current_thread()

   -- static field
   assert(t.MIN_PRIORITY == 1)

   -- private instance field
   -- TODO find a better field to use
   --assert(lj_toString(t) == lj_toString(t.me))

   print("ok")
end

function test_basic_method_call()
   io.write("Running test_basic_method_call().....")
   print("ok")
end

test_method_id_comparison()
test_basic_field_access()
test_basic_method_call()