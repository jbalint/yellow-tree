-- debuglib.lua
-- Commands implementing the debugger
-- Loaded automatically at debugger start

 --   _____                                          _     
 --  / ____|                                        | |    
 -- | |     ___  _ __ ___  _ __ ___   __ _ _ __   __| |___ 
 -- | |    / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` / __|
 -- | |___| (_) | | | | | | | | | | | (_| | | | | (_| \__ \
 --  \_____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_|___/

-- current stack depth
depth = 0

-- breakpoint list
breakpoints = {}

-- ============================================================
-- Print stack trace
-- ============================================================
function where()
   local frame_count = lj_get_frame_count()

   if frame_count == 0 then
      print("No code running")
      return
   end

   for i = 0, frame_count - 1 do
      print(stack_frame_to_string(lj_get_stack_frame(i)))
   end
end

-- ============================================================
-- Print local variables in current stack frame
-- ============================================================
function locals()
   local frame = lj_get_stack_frame(depth)
   local var_table = lj_get_local_variable_table(frame.method_id)
   for k, v in pairs(var_table) do
      local type = v.sig
      if (#v.sig > 1) then
	 type = "L"
      end
      print(string.format("%10s = %s", k,
			  lj_get_local_variable(depth, v.slot, type)))
   end
end

-- ============================================================
-- ============================================================
function next(num)
   num = num or 1
   -- TODO see step_next_line() in yt.c
end

-- ============================================================
-- ============================================================
function step(num)
   num = num or 1
   -- TODO see step_next_line() in yt.c
end

-- ============================================================
-- Move one frame up the stack
-- ============================================================
function up(num)
   num = num or 1
   frame(depth + num)
end

-- ============================================================
-- Move one frame down the stack
-- ============================================================
function down(num)
   num = num or 1
   frame(depth - num)
end

-- ============================================================
-- Set the stack depth
-- ============================================================
function frame(num)
   num = num or 0
   local frame_count = lj_get_frame_count()
   if num < 0 then
      print("Invalid frame number " .. num)
      depth = 0
   elseif num > frame_count - 1 then
      print("Invalid frame number " .. num)
      depth = frame_count - 1
   else
      depth = num
   end
      
   print(stack_frame_to_string(lj_get_stack_frame(depth)))
end

-- ============================================================
-- Add a new breakpoint
-- takes a method declaration, line number (can be 0)
-- ============================================================
function bp(method_decl, line_num)
   local b = {}
   b.method_decl = method_decl
   b.line_num = line_num or 0

   -- make sure bp doesn't already exist
   for idx, bp in ipairs(breakpoints) do
      if bp.method_decl == b.method_decl and bp.line_num == b.line_num then
	 print("Breakpoint already exists")
	 return
      end
   end

   b.method_id = lookup_method_id(method_decl)
   lj_set_breakpoint(b.method_id, b.line_num)
   table.insert(breakpoints, b)
   print("ok")
end

-- ============================================================
-- List breakpoint(s)
-- ============================================================
function bl(num)
   local function print_bp(idx, bp)
      local disp = string.format("%4d: %s", idx, bp.method_decl)
      if (bp.line_num) then
	 disp = disp .. " (line " .. bp.line_num .. ")"
      end
      print(disp)
   end

   -- print only one
   if num then
      local bp = breakpoints[num]
      if not bp then
	 print("Invalid breakpoint")
	 return
      end
      print_bp(num, bp)
      return
   end

   -- print all
   if #breakpoints == 0 then
      print("No breakpoints")
      return
   end
   for idx, bp in ipairs(breakpoints) do
      print_bp(idx, bp)
   end
end

-- ============================================================
-- Clear breakpoint(s)
-- ============================================================
function bc(num)
   -- TODO
end

 --  _    _ _   _ _     
 -- | |  | | | (_) |    
 -- | |  | | |_ _| |___ 
 -- | |  | | __| | / __|
 -- | |__| | |_| | \__ \
 --  \____/ \__|_|_|___/

-- ============================================================
-- jmethod_id metatable
jmethod_id_mt = {}
jmethod_id_mt.__tostring = function(method_id)
   return string.format("jmethod_id@%s", lj_pointer_to_string(method_id))
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
   end
end

jfield_id_mt = {}
jfield_id_mt.__tostring = function(field_id)
   return string.format("jfield_id@%s %s.%s type=%s",
			lj_pointer_to_string(field_id),
			field_id.class.getName().toString(),
			field_id.name,
		        field_id.sig)
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

function find_methods(class, name)
   -- search up the class hierarchy for methods
   local methods = {}
   local superclass_method_id = lj_get_method_id("java/lang/Class", "getSuperclass", "", "Ljava/lang/Class;")
   while class do
      for idx, method_id in pairs(lj_get_class_methods(class)) do
	 if method_id.name == name then
	    methods[#methods+1] = method_id
	 end
      end
      class = lj_call_method(class, superclass_method_id, "L", 0)
   end
   return methods
end

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

function new_jcallable_method(object, possible_methods)
   local jcm = {}
   local mt = {
      __call = function(...) return call_java_method(object, possible_methods, arg) end
   }
   return setmetatable(jcm, mt)
end

function call_java_method(object, possible_methods, args)
   local method_id = nil
   if #possible_methods == 1 then
      method_id = possible_methods[1]
   elseif #possible_methods > 1 then
      print("more than one method")
      method_id = possible_methods[1]
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

-- ============================================================
-- String format of stack frame
function stack_frame_to_string(f)
   local disp = string.format("%4d: %s.%s", f.depth, f.class, f.method)
   if f.depth == depth then
      disp = string.gsub(disp, ".", "*", 1)
   end
   if f.sourcefile then
      disp = disp .. "(" .. f.sourcefile
      if f.line_num then
	 disp = disp .. ":" .. f.line_num
      end
      disp = disp .. ")"
   else
      disp = disp .. "(" .. f.source .. ")"
   end
   return disp
end

-- ============================================================
-- Look up a method id from a method declaration
function lookup_method_id(method_decl)
   local method = method_decl_parse(method_decl)
   return lj_get_method_id(method.class, method.method, method.args, method.ret)
end

-- ============================================================
-- Parse a method declaration of the form
-- java/io/PrintStream.println(Ljava/lang/String;)V
function method_decl_parse(method_decl)
   local chars = 0
   local md = {}

   md.class = string.match(method_decl, "^.*%.") -- up until .
   chars = string.len(md.class)
   md.class = string.sub(md.class, 0, string.len(md.class) - 1) -- remove .

   md.method = string.match(method_decl, "^.*%(", chars + 1) -- up until (
   chars = chars + string.len(md.method)
   md.method = string.sub(md.method, 0, string.len(md.method) - 1) -- remove (

   md.args = string.match(method_decl, "^.*%)", chars + 1) -- up until )
   chars = chars + string.len(md.args)
   md.args = string.sub(md.args, 0, string.len(md.args) - 1) -- remove )

   md.ret = string.sub(method_decl, chars + 1) -- rest

   return md
end

-- ============================================================
-- http://snippets.luacode.org/?p=snippets/Simple_Table_Dump_7
-- fixed to print recursive tables
function dump(o)
   if type(o) == 'table' then
      local s = '{ ' .. "\n"
      for k,v in pairs(o) do
	 if type(k) == 'table' then
	    k = dump(k)
	 elseif type(k) ~= 'number' then
	    k = '"'..k..'"'
	 end
	 s = s .. '['..k..'] = ' .. dump(v) .. ','
	 s = s .. "\n"
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

print("debuglib.lua - loaded with " .. _VERSION)

-- internal function fors testing/dev
function x()
   xtoString = lj_get_method_id("java/lang/Object", "toString", "", "Ljava/lang/String;")
   xtoUpperCase = lj_get_method_id("java/lang/String", "toUpperCase", "", "Ljava/lang/String;")
   xconcat = lj_get_method_id("java/lang/String", "concat", "Ljava/lang/String;", "Ljava/lang/String;")
   bp("Test.b(I)V")
   bl()
   print(dump(lj_get_class_methods(lj_find_class("java/lang/String"))))
end

function y()
   local s = lj_call_method(lj_get_current_thread(), xtoString, "L", 0)
   print(s)
   local y = lj_call_method(s, xconcat, "L", 1, "STR", ", lol")
   print(y)
end

function run_test()
   loadfile("test_basic.lua")()
   loadfile("test_lua_java.lua")()
end

function z() -- throwing a lua assertion "not enough elements in the stack"
   local x = lj_get_current_thread()
   local y = lj_get_method_id("java/lang/Thread", "activeCount", "", "I")
   lj_call_method(x, y, "I", 0)
   print(lj_get_current_thread().getName())
end

function z1()
   local x = lj_get_current_thread()
   local y = lj_get_field_id("java/lang/Thread", "tid", "J")
   print(lj_get_field(x, y, false))
   local z = lj_get_field_id("java/lang/Thread", "MIN_PRIORITY", "I")
   print(z)
   print(dump(x.fields))
   print(dump(lj_get_field_modifiers_table(lj_get_field_modifiers(z))))
   print(lj_get_field(x.class, z, true))
   print(x.MIN_PRIORITY)
   print(x.MAX_PRIORITY)
   print(x.me)
end