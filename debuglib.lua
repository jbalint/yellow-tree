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
      local f = lj_get_stack_frame(i)
      local disp = string.format("%4d: %s.%s", i, f.class, f.method)
      if f.sourcefile then
	 disp = disp .. "(" .. f.sourcefile
	 if f.line_num then
	    disp = disp .. ":" .. f.line_num
	 end
	 disp = disp .. ")"
      else
	 disp = disp .. "(" .. f.source .. ")"
      end
      print(disp)
   end
end

-- ============================================================
-- ============================================================
function locals()
   -- TODO see dump_locals() in yt.c
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
-- ============================================================
function up(num)
   num = num or 1
   -- TODO adjust depth as necessary (take optional param)
   -- show current frame
   -- see frame_adjust() in yt.c
end

-- ============================================================
-- ============================================================
function down(num)
   num = num or 1
end

-- ============================================================
-- ============================================================
function frame(num)
end

-- ============================================================
-- Add a new breakpoint
-- takes a method declaration, line number (can be 0)
-- ============================================================
function bp(method_decl, line_num)
   local b = {}
   b.method_decl = method_decl
   b.line_num = line_num

   -- make sure bp doesn't already exist
   for idx, bp in ipairs(breakpoints) do
      if bp.method_decl == b.method_decl and bp.line_num == b.line_num then
	 print("Breakpoint already exists")
	 return
      end
   end

   b.method = method_decl_parse(method_decl)
   lj_set_breakpoint(b.method, b.line_num)
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
-- jthread metatable
jthread_mt = {}
jthread_mt.__tostringUNUSED = function(t)
   -- TODO need the pointer value here
   return "jthread<jvmti pointer>"
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