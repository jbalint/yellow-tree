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

-- ============================================================
-- search up the class hierarchy for methods called `name'
local function find_methods(class, name)
   local methods = {}
   local superclass_method_id = lj_get_method_id("java/lang/Class", "getSuperclass", "", "Ljava/lang/Class;")
   while class do
      for idx, method_id in pairs(lj_get_class_methods(class)) do
	 -- match literal method names or "new" for constructors
	 if method_id.name == name or (method_id.name == "<init>" and name == "new") then
	    table.insert(methods, method_id)
	 end
      end
      class = lj_call_method(class, superclass_method_id, "L", 0)
   end
   return methods
end

-- ============================================================
-- search up the class hierarchy for the first field called `name'
local function find_field(class, name)
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

local function get_ret_type(method_id)
   local ret = method_id.ret
   -- prefer to return a native string ONLY for this method
   if method_id.name == "toString" then
      ret = "STR"
   elseif string.sub(ret, 1, 1) == "L" then
      ret = "L"
   -- return an object for constructors
   elseif method_id.name == "<init>" then
      ret = "L"
   end
   return ret
end

-- ============================================================
-- parse an arg spec from a method signature into individual components
local function parse_arg_spec(argspec)
   -- TODO array support needed here
   if argspec == "" then return {} end
   local argarray = {}
   while argspec ~= "" do
      local char1 = string.sub(argspec, 1, 1)
      if char1 == "L" then
	 local endi = string.find(argspec, ";")
	 table.insert(argarray, string.sub(argspec, 1, endi))
	 argspec = string.sub(argspec, endi + 1)
      else
	 -- assume primitive type (single char)
	 table.insert(argarray, char1)
	 argspec = string.sub(argspec, 2)
      end
   end
   return argarray
end

-- ============================================================
-- perform the actual method call. this will match the `args'
-- to one of the `possible_methods'
local function call_java_method(object, possible_methods, args)
   -- local old_lj = lj_call_method
   -- local function lj_call_method(a, b, c, d)
   --    print("Calling: ", b.class, b.name)
   --    return old_lj(a, b, c, d)
   -- end
   local argc = #args
   -- filter out non-matching methods
   local possible2 = {}
   --.... this loop only exists because there is no "continue" in lua
   for i,m in ipairs(possible_methods) do
      -- short circuit match for no args
      if #args == 0 and m.args == "" then
	 return lj_call_method(object, m, get_ret_type(m), 0)
      end

      -- we can't handle array args, so....
      if not string.find(m.args, "[", 1, true) and
	 -- only try to match methods with the same number of arguments
	 #parse_arg_spec(m.args) == argc then
	 table.insert(possible2, m)
      end
   end

   -- try to match types with args
   local jargs = {}
   for i, m in ipairs(possible2) do
      local atypes = parse_arg_spec(m.args)
      for i, t in ipairs(atypes) do
	 local argi = args[i]
	 if argi == nil then
	    table.insert(jargs, "V")
	    table.insert(jargs, nil)
	 elseif t == "Ljava/lang/String;" and type(argi) ~= "userdata" then
	    table.insert(jargs, "STR")
	    table.insert(jargs, string.format("%s", argi))
	 elseif string.sub(t, 1, 1) == "L" and type(argi) == "userdata" and
	    string.find(string.format("%s", argi), "jobject@") == 1 then
	    local tc = lj_find_class(string.sub(t, 2, #t - 1))
	    if tc.isAssignableFrom(argi.class) then
	       table.insert(jargs, "L")
	       table.insert(jargs, argi)
	    end
	 elseif (t == "I" or t == "J") and type(argi) == "number" then
	    table.insert(jargs, t)
	    table.insert(jargs, math.floor(argi))
	 elseif (t == "F" or t == "D") and type(argi) == "number" then
	    table.insert(jargs, t)
	    table.insert(jargs, argi)
	 end

	 -- call only if all args matched
	 if #jargs == (argc * 2) then
	    return lj_call_method(object, m, get_ret_type(m), argc, unpack(jargs))
	 end
      end
      --print("more than one possible method, using: " .. method_id.name .. " from " .. lj_toString(method_id.class))
   end
   error("No matching method for given arguments")
end

-- ============================================================
-- create a callable closure to call one of the
-- `possible_methods' on `object'
local function new_jcallable_method(object, possible_methods)
   local jcm = {}
   local mt = {
      __call = function(t, ...) return call_java_method(object, possible_methods, {...}) end
   }
   return setmetatable(jcm, mt)
end

-- ============================================================
-- jmethod_id metatable
local jmethod_id_mt = {}
debug.getregistry()["jmethod_id_mt"] = jmethod_id_mt
jmethod_id_mt.__tostring = function(method_id)
   return string.format("jmethod_id@%s %s.%s%s",
			lj_pointer_to_string(method_id),
			method_id.class.name,
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
   elseif k == "modifiers" then
      return lj_get_method_modifiers_table(lj_get_method_modifiers(method_id))
   end
end

-- ============================================================
-- jfield_id metatable
local jfield_id_mt = {}
debug.getregistry()["jfield_id_mt"] = jfield_id_mt
jfield_id_mt.__tostring = function(field_id)
   return string.format("jfield_id@%s %s.%s type=%s",
			lj_pointer_to_string(field_id),
			field_id.class.name,
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
local jobject_mt = {}
debug.getregistry()["jobject_mt"] = jobject_mt
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

   -- special fields for "class" objects
   if lj_toString(class) == "class java.lang.Class" then
      if key == "sourcefile" then
	 return lj_get_source_filename(object)
      elseif key == "name" then
	 -- name is a transient and cached property in java.lang.Class, don't access it directly
	 -- c.f. Class.java source
	 return lj_call_method(object,
			       lj_get_method_id("java/lang/Class",
						"getName",
						"", "Ljava/lang/String;"),
			       "STR", 0)
      elseif key == "isAssignableFrom" then
	 -- handled manually to prevent recursion in generic method calling
	 local isAssignableFromMethod = function(c2)
	    return lj_call_method(object,
				  lj_get_method_id("java/lang/Class",
						   "isAssignableFrom",
						   "Ljava/lang/Class;", "Z"),
				  "Z", 1, "Ljava/lang/Class;", c2)
	 end
	 return isAssignableFromMethod
      end
      -- NOTE: following this, object == class
      class = object
   end

   local field_id = find_field(class, key)
   if field_id then
      return lj_get_field(object, field_id, field_id.modifiers.static)
   end

   local methods = find_methods(class, key)
   if #methods > 0 then
      return new_jcallable_method(object, methods)
   end
end

print("java_bridge.lua - loaded with " .. _VERSION)
