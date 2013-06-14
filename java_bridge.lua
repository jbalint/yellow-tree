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
      class = lj_call_method(class, superclass_method_id, false, "L", 0)
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
      class = lj_call_method(class, superclass_method_id, false, "L", 0)
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
   if argspec == "" then return {} end
   local argarray = {}
   while argspec ~= "" do
      local char1 = string.sub(argspec, 1, 1)
      local char2 = string.sub(argspec, 2, 2)
      if char1 == "L" or (char1 == "[" and char2 == "L") then
	 -- object / object array
	 local endi = string.find(argspec, ";")
	 table.insert(argarray, string.sub(argspec, 1, endi))
	 argspec = string.sub(argspec, endi + 1)
      elseif char1 == "[" then
	 -- primitive array
	 table.insert(argarray, char1 .. char2)
	 argspec = string.sub(argspec, 3)
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
   local argc = #args
   -- filter out non-matching methods
   local possible2 = {}
   --.... this loop only exists because there is no "continue" in lua
   for i,m in ipairs(possible_methods) do
      -- short circuit match for no args
      if #args == 0 and m.args == "" then
	 if m.modifiers.static then
	    object = object.class
	 end
	 return lj_call_method(object, m, m.modifiers.static, get_ret_type(m), 0)
      end

      -- only try to match methods with the same number of arguments
      if #parse_arg_spec(m.args) == argc then
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
	 elseif (t == "Z") and type(argi) == "boolean" then
	    table.insert(jargs, t)
	    table.insert(jargs, argi)
	 elseif (t == "I" or t == "J") and type(argi) == "number" then
	    table.insert(jargs, t)
	    table.insert(jargs, math.floor(argi))
	 elseif (t == "F" or t == "D") and type(argi) == "number" then
	    table.insert(jargs, t)
	    table.insert(jargs, argi)
	 elseif string.sub(t, 1, 1) == "[" and type(argi) == "userdata" and argi.class.name == t then
	    table.insert(jargs, "[")
	    table.insert(jargs, argi)
	 elseif t == "Ljava/lang/String;" and type(argi) ~= "userdata" then
	    table.insert(jargs, "STR")
	    table.insert(jargs, string.format("%s", argi))
	 elseif (string.sub(t, 1, 1) == "L" or string.sub(t, 1, 2) == "[L") and type(argi) == "userdata" and string.find(string.format("%s", argi), "jobject@") == 1 then
	    local name = t
	    -- from L; for non-arrays
	    if string.sub(t, 1, 1) == "L" then
	       name = string.sub(t, 2, #t - 1)
	    end
	    local tc = lj_find_class(name)
	    assert(tc)
	    if tc.isAssignableFrom(argi.class) then
	       table.insert(jargs, "L")
	       table.insert(jargs, argi)
	    end
	 end

	 -- call only if all args matched
	 if #jargs == (argc * 2) then
	    if m.modifiers.static then
	       object = object.class
	    end
	    return lj_call_method(object, m, m.modifiers.static, get_ret_type(m), argc, unpack(jargs))
	 end
      end
      --print("more than one possible method, using: " .. method_id.name .. " from " .. lj_toString(method_id.class))
   end
   error("No matching method for given arguments: " .. dump(possible_methods))
end

-- ============================================================
-- create a callable closure to call one of the
-- `possible_methods' on `object'
local function new_jcallable_method(object, possible_methods)
   local jcm = {}
   jcm.possible_methods = possible_methods
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
   elseif k == "line_number_table" then
      return lj_get_line_number_table(method_id)
   elseif k == "local_variable_table" then
      return lj_get_local_variable_table(method_id)
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
   return f1.name == f2.name and
      f1.sig == f2.sig and
      f1.class == f2.class
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
-- jmonitor metatable - created by lj_create_raw_monitor()
local jmonitor_mt = {}
debug.getregistry()["jmonitor_mt"] = jmonitor_mt
jmonitor_mt.__tostring = function(monitor)
   return string.format("jmonitor@%s",
			lj_pointer_to_string(monitor))
end
jmonitor_mt.__index = jmonitor_mt
function jmonitor_mt:destroy()
   lj_destroy_raw_monitor(self)
end
function jmonitor_mt:lock()
   lj_raw_monitor_enter(self)
end
function jmonitor_mt:unlock()
   lj_raw_monitor_exit(self)
end
function jmonitor_mt:wait(time)
   lj_raw_monitor_wait(self, time or 0)
end
function jmonitor_mt:notify()
   lj_raw_monitor_notify(self)
end
function jmonitor_mt:broadcast()
   lj_raw_monitor_notify_all(self)
end
function jmonitor_mt:wait_without_lock(time)
   self:lock()
   self:wait(time)
   self:unlock()
end
function jmonitor_mt:notify_without_lock()
   self:lock()
   self:notify()
   self:unlock()
end
function jmonitor_mt:broadcast_without_lock()
   self:lock()
   self:broadcast()
   self:unlock()
end

-- ============================================================
-- jobject metatable
local jobject_mt = {}
setmetatable(jobject_mt, jobject_mt)
debug.getregistry()["jobject_mt"] = jobject_mt
jobject_mt.__tostring = function(object)
   return string.format("jobject@%s: %s",
			lj_pointer_to_string(object),
			lj_toString(object))
end
jobject_mt.__eq = function(o1, o2)
   local o1c, o2c = lj_toString(o1.class), lj_toString(o2.class)
   if o1c ~= o2c then return false end

   -- Class and String comparisons
   if o1c == "class java.lang.Class" or o1c == "class java.lang.String" then
      return lj_toString(o1) == lj_toString(o2)
   end

   -- TODO reference comparison? possible? useful? equals?
   return false
end
jobject_mt.__len = function(object)
   if object.class.name:sub(1, 1) == "[" then
      return lj_get_array_length(object)
   end
   return 0
end
jobject_mt.__index = function(object, key)
   -- we cannot use anything that would result in calling this function recursively
   local getclass_method_id = lj_get_method_id("java/lang/Object", "getClass", "", "Ljava/lang/Class;")
   local class = lj_call_method(object, getclass_method_id, false, "L", 0)
   local class_name = string.sub(lj_toString(class), 7)

   if key == "class" then
      return class
   end

   -- special fields for arrays
   if string.find(class_name, "[", 1, true) == 1 then
      if key == "length" then
	 return lj_get_array_length(object)
      elseif type(key) == "number" then
	 return lj_get_array_element(object, class_name, key)
      end
   end

   -- special fields for "thread" objects
   if class_name == "java.lang.Thread" then
      if key == "frame_count" then
	 return lj_get_frame_count(object)
      elseif key == "frames" then
	 local frames = {}
	 for i = 1, object.frame_count do
	    table.insert(frames, lj_get_stack_frame(i, object))
	 end
	 return frames
      end
   end

   -- special fields for "class" objects
   if class_name == "java.lang.Class" then
      if key == "sourcefile" then
	 return lj_get_source_filename(object)
      elseif key == "name" then
	 -- name is a transient and cached property in java.lang.Class, don't access it directly
	 -- c.f. Class.java source
	 return lj_call_method(object,
			       lj_get_method_id("java/lang/Class",
						"getName",
						"", "Ljava/lang/String;"),
			       false, "STR", 0)
      elseif key == "isAssignableFrom" then
	 -- handled manually to prevent recursion in generic method calling
	 local isAssignableFromMethod = function(c2)
	    return lj_call_method(object,
				  lj_get_method_id("java/lang/Class",
						   "isAssignableFrom",
						   "Ljava/lang/Class;", "Z"),
				  false, "Z", 1, "Ljava/lang/Class;", c2)
	 end
	 return isAssignableFromMethod
      end
      -- NOTE: following this, object == class
      class = object
   end

   if key == "fields" then
      local fields = {}
      -- index by name
      for idx, field in pairs(lj_get_class_fields(class)) do
	 fields[field.name] = field
      end
      return fields
   end

   if key == "methods" then
      local methods = {}
      -- index by name, sig
      for idx, method in pairs(lj_get_class_methods(class)) do
	 methods[method.name .. method.sig] = method
      end
      return methods
   end

   local field_id = find_field(class, key)
   if field_id then
      return lj_get_field(object, field_id, field_id.modifiers.static)
   end

   local methods = find_methods(class, key)
   if #methods > 0 then
      return new_jcallable_method(object, methods)
   end

   return jobject_mt[key]
end
function jobject_mt:global_ref()
   return lj_convert_to_global_ref(self)
end

print("java_bridge.lua - loaded with " .. _VERSION)
