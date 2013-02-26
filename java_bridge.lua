 --       _                    ____       _     _            
 --      | |                  |  _ \     (_)   | |           
 --      | | __ ___   ____ _  | |_) |_ __ _  __| | __ _  ___ 
 --  _   | |/ _` \ \ / / _` | |  _ <| '__| |/ _` |/ _` |/ _ \
 -- | |__| | (_| |\ V / (_| | | |_) | |  | | (_| | (_| |  __/
 --  \____/ \__,_| \_/ \__,_| |____/|_|  |_|\__,_|\__, |\___|
 --                                                __/ |     
 --                                               |___/      

-- Bridge Java objects to be accessible in Lua
-- required by lua_java.c

module("java_bridge", package.seeall)

-- ============================================================
-- jmethod_id metatable
jmethod_id_mt = {}
jmethod_id_mt.__tostring = function(method_id)
   return string.format("jmethod_id@%s %s.%s%s",
			lj_pointer_to_string(method_id),
			method_id.class.name.toString(),
			method_id.name,
			method_id.sig)
end
jmethod_id_mt.__eq = function(m1, m2)
   return m1.name == m2.name and
      m1.sig == m2.sig and
      m1.class == m2.class
end
jmethod_id_mt.__index = function(method_id, k)
   if k == "name" then
      return lj_get_method_name(method_id).name
   elseif k == "sig" then
      return lj_get_method_name(method_id).sig
   elseif k == "class" then
      return lj_get_method_declaring_class(method_id)
   elseif k == "args" or k == "ret" then
      local sig = lj_get_method_name(method_id).sig
      sig = string.sub(sig, 2) -- remove (
      local args = string.match(sig, "^.*%)")
      args = string.sub(args, 1, string.len(args) - 1)
      local ret = string.match(sig, "%).*$")
      ret = string.sub(ret, 2)
      if k == "args" then
	 return args
      elseif k == "ret" then
	 return ret
      end
   elseif k == "file" then
      return lj_get_source_filename(method_id.class)
   elseif k == "line_number_table" then
      local lnt = lj_get_line_number_table(method_id)
      if not lnt then
	 --print(string.format("line_number_table nil: %s", method_id))
      end
      return lnt
   elseif k == "local_variable_table" then
      local locals = lj_get_local_variable_table(method_id)
      if not locals then
	 --print(string.format("local_variable_table nil: %s", method_id))
      end
      return locals
   end
end

-- ============================================================
-- jfield_id metatable
jfield_id_mt = {}
jfield_id_mt.__tostring = function(field_id)
   return string.format("jfield_id@%s %s.%s type=%s",
			lj_pointer_to_string(field_id),
			field_id.class.name.toString(),
			field_id.name,
		        field_id.sig)
end
jfield_id_mt.__eq = function(f1, f2)
   return m1.name == m2.name and
      m1.sig == m2.sig and
      m1.class == m2.class
end
jfield_id_mt.__index = function(field_id, k)
   if k == "name" then
      return lj_get_field_name(field_id).name
   elseif k == "sig" then
      return lj_get_field_name(field_id).sig
   elseif k == "class" then
      return lj_get_field_declaring_class(field_id)
   elseif k == "modifiers" then
      return lj_get_field_modifiers_table(lj_get_field_modifiers(field_id))
   end
end

-- ============================================================
-- jobject metatable
jobject_mt = {}
jobject_mt.__tostring = function(object)
   return string.format("jobject@%s: %s",
			lj_pointer_to_string(object),
			lj_toString(object))
end
jobject_mt.__eq = function(o1, o2)
   -- only handle class comparison
   if lj_toString(o1.class) == "class java.lang.Class" then
      return lj_toString(o1) == lj_toString(o2)
   end

   return false
end
jobject_mt.__index = function(object, key)
   -- we cannot use anything that would result in calling this function recursively
   local getclass_method_id = lj_get_method_id("java/lang/Object", "getClass", "", "Ljava/lang/Class;")
   local class = lj_call_method(object, getclass_method_id, "L", 0)

   if key == "class" then
      return class
   end

   if key == "fields" then
      return lj_get_class_fields(class)
   end

   if key == "methods" then
      return lj_get_class_methods(class)
   end

   -- special field for "class" objects
   if lj_toString(class) == "class java.lang.Class" then
      if key == "sourcefile" then
	 return lj_get_source_filename(object)
      end
   end

   local methods = find_methods(class, key)
   if #methods > 0 then
      return new_jcallable_method(object, methods)
   end

   local field_id = find_field(class, key)
   if field_id then
      if field_id.modifiers.static then
	 return lj_get_field(object.class, field_id, true)
      else
	 return lj_get_field(object, field_id, false)
      end
   end
end

-- ============================================================
-- search up the class hierarchy for methods called `name'
function find_methods(class, name)
   local methods = {}
   local superclass_method_id = lj_get_method_id("java/lang/Class", "getSuperclass", "", "Ljava/lang/Class;")
   while class do
      for idx, method_id in pairs(lj_get_class_methods(class)) do
	 if method_id.name == name then
	    --print(name .. " match for class " .. lj_toString(class))
	    methods[#methods+1] = method_id
	 end
      end
      class = lj_call_method(class, superclass_method_id, "L", 0)
   end
   return methods
end

-- ============================================================
-- search up the class hierarchy for the first field called `name'
function find_field(class, name)
   local superclass_method_id = lj_get_method_id("java/lang/Class", "getSuperclass", "", "Ljava/lang/Class;")
   while class do
      for idx, field_id in pairs(lj_get_class_fields(class)) do
	 if field_id.name == name then
	    return field_id
	 end
      end
      class = lj_call_method(class, superclass_method_id, "L", 0)
   end
   return nil
end

-- ============================================================
-- create a callable closure to call one of the
-- `possible_methods' on `object'
function new_jcallable_method(object, possible_methods)
   local jcm = {}
   local mt = {
      __call = function(...) return call_java_method(object, possible_methods, arg) end
   }
   return setmetatable(jcm, mt)
end

-- ============================================================
-- perform the actual method call. this will match the `args'
-- to one of the `possible_methods'
function call_java_method(object, possible_methods, args)
   local method_id = nil
   if #possible_methods == 1 then
      method_id = possible_methods[1]
   elseif #possible_methods > 1 then
      method_id = possible_methods[1]
      --print("more than one possible method, using: " .. method_id.name .. " from " .. lj_toString(method_id.class))
   end
   local ret = method_id.ret
   -- prefer to return a native string ONLY for this method
   if method_id.name == "toString" then
      ret = "STR"
   elseif string.sub(ret, 1, 1) == "L" then
      ret = "L"
   end
   return lj_call_method(object, method_id, ret, 0)
end

print("java_bridge.lua - loaded with " .. _VERSION)
