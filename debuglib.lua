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
depth = 1

-- breakpoint list
breakpoints = {}

-- ============================================================
-- Main command loop
-- ============================================================
function command_loop()
   while true do
      io.write("yt> ")
      io.flush()
      local cmd = io.read("*line")
      local chunk = load(cmd)
      local success, m2 = pcall(chunk)
      if not success then
	 print("Error: " .. m2)
      end
   end
end

-- ============================================================
-- Continue execution
-- ============================================================
function g()
   lj_resume_jvm_and_wait()
end

-- ============================================================
-- Help
-- ============================================================
function help()
   print("Help is on the way...")
end

-- ============================================================
-- Print stack trace
-- ============================================================
function where()
   local frame_count = lj_get_frame_count()

   if frame_count == 0 then
      print("No code running")
      return nil
   end

   local stack = {}
   for i = 1, frame_count do
      local f = lj_get_stack_frame(i)
      stack[i] = f
      print(stack_frame_to_string(f))
   end
   return stack
end

-- ============================================================
-- Print local variables in current stack frame
-- ============================================================
function locals()
   local frame = lj_get_stack_frame(depth)
   local var_table = lj_get_local_variable_table(frame.method_id)
   for k, v in pairs(var_table) do
      print(string.format("%10s = %s", k,
			  lj_get_local_variable(depth, v.slot, v.sig)))
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
   return frame(depth + num)
end

-- ============================================================
-- Move one frame down the stack
-- ============================================================
function down(num)
   num = num or 1
   return frame(depth - num)
end

-- ============================================================
-- Set the stack depth
-- ============================================================
function frame(num)
   num = num or 1
   local frame_count = lj_get_frame_count()
   if num < 1 then
      print("Invalid frame number " .. num)
      depth = 1
   elseif num > frame_count then
      print("Invalid frame number " .. num)
      depth = frame_count
   else
      depth = num
   end

   local f = lj_get_stack_frame(i)
   print(stack_frame_to_string(f))
   return f
end

-- ============================================================
-- Add a new breakpoint
-- takes a method declaration, line number (can be 0)
-- ============================================================
function bp(method_decl, line_num)
   local b = {}
   b.method_decl = method_decl
   b.line_num = line_num or 0
   (getmetatable(b) or (setmetatable(b, {}) and getmetatable(b))).__tostring = function(bp)
      local disp = string.format("%s", bp.method_decl)
      if (bp.line_num) then
	 disp = disp .. " (line " .. bp.line_num .. ")"
      end
      return disp
   end

   -- make sure bp doesn't already exist
   for idx, bp in ipairs(breakpoints) do
      if bp.method_decl == b.method_decl and bp.line_num == b.line_num then
	 print("Breakpoint already exists")
	 return
      end
   end

   local method = method_decl_parse(method_decl)
   b.method_id = lj_get_method_id(method.class, method.method, method.args, method.ret)
   lj_set_breakpoint(b.method_id, b.line_num)
   table.insert(breakpoints, b)
   print("ok")

   return b
end

-- ============================================================
-- List breakpoint(s)
-- ============================================================
function bl(num)
   -- print only one
   if num then
      local bp = breakpoints[num]
      if not bp then
	 print("Invalid breakpoint")
	 return
      end
      print(string.format("%4d: %s", num, bp))
      return bp
   end

   -- print all
   if #breakpoints == 0 then
      print("No breakpoints")
      return
   end
   for idx, bp in ipairs(breakpoints) do
      bl(idx)
   end

   return breakpoints
end

-- ============================================================
-- Clear breakpoint(s)
-- ============================================================
function bc(num)
   -- TODO
end

 --       ___      ____  __ _______ _____    _____      _ _ _                _        
 --      | \ \    / /  \/  |__   __|_   _|  / ____|    | | | |              | |       
 --      | |\ \  / /| \  / |  | |    | |   | |     __ _| | | |__   __ _  ___| | _____ 
 --  _   | | \ \/ / | |\/| |  | |    | |   | |    / _` | | | '_ \ / _` |/ __| |/ / __|
 -- | |__| |  \  /  | |  | |  | |   _| |_  | |___| (_| | | | |_) | (_| | (__|   <\__ \
 --  \____/    \/   |_|  |_|  |_|  |_____|  \_____\__,_|_|_|_.__/ \__,_|\___|_|\_\___/

-- ============================================================
-- Handle the callback when a breakpoint is hit
-- ============================================================
function cb_breakpoint(thread, method_id, location)
   depth = 1
   local bp
   for idx, v in pairs(breakpoints) do
      if v.method_id == method_id then
	 bp = v
      end
   end
   assert(bp)
   print(stack_frame_to_string(lj_get_stack_frame(1)))
   return true
end

function init_jvmti_callbacks()
   lj_set_jvmti_callback("breakpoint", cb_breakpoint)
end

 --  _    _ _   _ _     
 -- | |  | | | (_) |    
 -- | |  | | |_ _| |___ 
 -- | |  | | __| | / __|
 -- | |__| | |_| | \__ \
 --  \____/ \__|_|_|___/

-- ============================================================
-- make locals available throughout
function init_locals_environment()
   local mt = getmetatable(_ENV) or (setmetatable(_ENV, {}) and getmetatable(_ENV))
   mt.__index = function(t, k)
      local frame = lj_get_stack_frame(depth)
      if not frame then
	 return
      end
      local locals = lj_get_local_variable_table(frame.method_id)
      local l = locals[k]
      if l then
	 return lj_get_local_variable(depth, l.slot, l.sig)
      end
   end
   mt.__newindex = function(t, k, v)
      rawset(t, k, v)
   end
end

-- ============================================================
-- String format of stack frame
function stack_frame_to_string(f)
   -- look up line number
   local line_num = 0
   for idx, ln in ipairs(f.method_id.line_number_table) do
      if f.location >= ln.start_loc then
	 line_num = ln.line_num
      else
	 break
      end
   end

   local disp = string.format("%6s %s.%s%s - %s (%s:%s)",
			      "[" .. f.depth .. "]",
			      f.method_id.class.getName().toString(),
			      f.method_id.name,
			      f.method_id.sig,
			      f.location,
			      f.method_id.class.sourcefile or "<unknown>",
			      line_num)
   if f.depth == depth then
      disp = string.gsub(disp, ".", "*", 1)
   end

   return disp
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

init_locals_environment()
init_jvmti_callbacks()
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

function run_tests()
   loadfile("test/test_basic.lua")()
   loadfile("test/test_lua_java.lua")()
   loadfile("test/test_java_bridge.lua")()
end
