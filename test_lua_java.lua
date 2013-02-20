 --  _                      _                    _______        _       
 -- | |                    | |                  |__   __|      | |      
 -- | |    _   _  __ _     | | __ ___   ____ _     | | ___  ___| |_ ___ 
 -- | |   | | | |/ _` |_   | |/ _` \ \ / / _` |    | |/ _ \/ __| __/ __|
 -- | |___| |_| | (_| | |__| | (_| |\ V / (_| |    | |  __/\__ \ |_\__ \
 -- |______\__,_|\__,_|\____/ \__,_| \_/ \__,_|    |_|\___||___/\__|___/

print("**************************")
print("Running lua_java tests....")

function test_find_class()
   io.write("Running test_find_class().....")
   local string_class = lj_find_class("java/lang/String")
   assert(string_class)
   local class_name = lj_toString(string_class)
   assert(class_name == "class java.lang.String")
   print("ok")
end

function test_get_declared_methods()
   -- TODO not a great test
   io.write("Running test_get_declared_methods().....")
   local fos_class = lj_find_class("java/io/FilterOutputStream")
   assert(fos_class)
   local methods = lj_get_class_methods(fos_class)
   local function s(m1, m2)
      if m1.name < m2.name then
	 return true
      else
	 return false
      end
   end
   table.sort(methods, s)
   assert(methods[1].name == "<init>")
   assert(methods[2].name == "close")
   print("ok")
end

test_find_class()
test_get_declared_methods()