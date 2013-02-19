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
   return "jmethod_id@" .. lj_pointer_to_string(method_id)
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
      local s = '{ '
      for k,v in pairs(o) do
	 if type(k) == 'table' then
	    k = dump(k)
	 elseif type(k) ~= 'number' then
	    k = '"'..k..'"'
	 end
	 s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

print("debuglib.lua - loaded with " .. _VERSION)

-- internal function for testing/dev
function x()
   bp("Test.b(I)V")
   bl()
end