-- debuglib.lua
-- Commands implementing the debugger
-- Loaded automatically at debugger start

 --   _____                                          _     
 --  / ____|                                        | |    
 -- | |     ___  _ __ ___  _ __ ___   __ _ _ __   __| |___ 
 -- | |    / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` / __|
 -- | |___| (_) | | | | | | | | | | | (_| | | | | (_| \__ \
 --  \_____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_|___/

require("java_bridge")

-- options
options = {}

-- current stack depth
depth = 1

-- breakpoint list
breakpoints = {}

-- single step tracking
single_step_location = nil
single_step_method_id = nil

-- i/o
dbgio = require("console_io")

-- ============================================================
-- Starts debugger
-- ============================================================
function start()
   if options.runfile then
      local success, m2 = pcall(loadfile(options.runfile))
      if not success then
	 dbgio:print(string.format("Error running '%s': %s", options.runfile, m2))
      end
      g()
      return
   end
   dbgio:command_loop()
end

-- ============================================================
-- Continue execution
-- ============================================================
function g()
   lj_jvm_resume()
   -- TODO ... O.o debugging (oasis3de) won't work startup
   -- without this... no idea what's interfering (stdin...?)
   os.execute("sleep " .. tonumber(1))
end

-- ============================================================
-- Help
-- ============================================================
function help()
   dbgio:print("Help is on the way...")
end

-- ============================================================
-- Print stack trace
-- ============================================================
function where()
   local frame_count = lj_get_frame_count()

   if frame_count == 0 then
      dbgio:print("No code running")
      return nil
   end

   local stack = {}
   for i = 1, frame_count do
      local f = lj_get_stack_frame(i)
      stack[i] = f
      -- TODO limit frame count to prevent printing unreasonably large stacks
      dbgio:print(stack_frame_to_string(f))
   end
   return stack
end

-- ============================================================
-- Print local variables in current stack frame
-- ============================================================
function locals()
   local frame = lj_get_stack_frame(depth)
   local var_table = frame.method_id.local_variable_table
   for k, v in pairs(var_table) do
      dbgio:print(string.format("%10s = %s", k,
				 lj_get_local_variable(depth, v.slot, v.sig)))
   end
end

-- ============================================================
-- ============================================================
function next(num)
   num = num or 1

   -- find location of next line
   local f = lj_get_stack_frame(depth)
   local line_nums = f.method_id.line_number_table
   for idx, ln in ipairs(line_nums) do
      if f.location < ln.location then
	 single_step_location = ln.location
	 single_step_method_id = f.method_id
	 break
      end
   end

   -- TODO allow stepping up a frame....
   if not single_step_location then
      dbgio:print("Cannot step to next line")
      return nil
   end

   lj_set_jvmti_callback("single_step", cb_single_step)
   g()
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
      dbgio:print("Invalid frame number " .. num)
      depth = 1
   elseif num > frame_count then
      dbgio:print("Invalid frame number " .. num)
      depth = frame_count
   else
      depth = num
   end

   local f = lj_get_stack_frame(depth)
   dbgio:print(stack_frame_to_string(f))
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
	 dbgio:print("Breakpoint already exists")
	 return
      end
   end

   local method = method_decl_parse(method_decl)
   b.method_id = lj_get_method_id(method.class, method.method, method.args, method.ret)
   lj_set_breakpoint(b.method_id, b.line_num)
   table.insert(breakpoints, b)
   dbgio:print("ok")

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
	 dbgio:print("Invalid breakpoint")
	 return
      end
      dbgio:print(string.format("%4d: %s", num, bp))
      return bp
   end

   -- print all
   if #breakpoints == 0 then
      dbgio:print("No breakpoints")
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
   dbgio:print()
   dbgio:print(stack_frame_to_string(lj_get_stack_frame(1)))
   if bp.handler then
      -- allow bp handler if present
      return bp:handler()
   else
      -- else default to breaking to command loop
      return true
   end
end

function cb_method_entry(thread, method_id)
end

function cb_method_exit(thread, method_id, was_popped_by_exception, return_value)
end

function cb_single_step(thread, method_id, location)
   if single_step_location and
      location >= single_step_location and
      single_step_method_id == method_id then
      lj_clear_jvmti_callback("single_step")
      single_step_location = nil
      single_step_method_id = nil
      dbgio:print(stack_frame_to_string(lj_get_stack_frame(depth)))
      return true
   else
      return false
   end
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
-- parse debugger options
-- formatted like opt1=val1,opt2=val2
function setopts(optstring)
   if #optstring == 0 then
      return
   end
   -- separator location or nil if single param
   local seploc = string.find(optstring, ",")
   local opt
   if seploc then
      opt = string.sub(optstring, 1, seploc - 1)
      setopts(string.sub(optstring, seploc + 1))
   else
      opt = optstring
   end
   -- parse option key and value
   seploc = string.find(opt, "=")
   if not seploc then
      dbgio:print("Invalid option: ", opt)
      return
   end
   options[string.sub(opt, 1, seploc - 1)] = string.sub(opt, seploc + 1, -1)
end

-- ============================================================
-- make locals available throughout
function init_locals_environment()
   local mt = getmetatable(_ENV) or (setmetatable(_ENV, {}) and getmetatable(_ENV))
   mt.__index = function(t, k)
      local frame = lj_get_stack_frame(depth)
      if not frame then
	 return
      end
      local locals = frame.method_id.local_variable_table
      if not locals then
	 return
      end
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
   local line_num = -1
   for idx, ln in ipairs(f.method_id.line_number_table or {}) do
      if f.location >= ln.location then
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
   test_bp = bp("Test.b(I)V")
   test_bp.handlerX = function(bp)
      dbgio:print("test_bp.handler")
      dbgio:print(string.format("bp=%s", bp))
      dbgio:print(string.format("a=%s", a))
      return false
   end
   bl()
   dbgio:print(dump(lj_get_class_methods(lj_find_class("java/lang/String"))))
   g()
   print(this.polyMorphic1())
   print(this.polyMorphic2(4))
   print(this.polyMorphic3(19283))
   print(this.polyMorphic3("99"))
   print(this.polyMorphic4(12, this))
   print(this.polyMorphic5(17, this))
end

function run_tests()
   loadfile("test/test_basic.lua")()
   loadfile("test/test_lua_java.lua")()
   loadfile("test/test_java_bridge.lua")()
end
