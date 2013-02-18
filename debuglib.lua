-- debuglib.lua
-- Commands implementing the debugger
-- Loaded automatically at debugger start

-- current stack depth
depth = 0

-- breakpoint list
breakpoints = {}

function test()
   print("Testing function")
end

function where()
   local frame_count = lj_get_frame_count()
   for i = 0, frame_count - 1 do
      f = lj_get_stack_frame(i)
      dump(f)
   end
end

-- takes a method declaration and an optional line number
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

-- List breakpoints
function bl()
   for idx, bp in ipairs(breakpoints) do
      local disp = string.format("%4d: %s", idx, bp.method_decl)
      if (bp.line_num) then
	 disp = disp .. " (line " .. bp.line_num .. ")"
      end
      print(disp)
   end
end

-- Clear breakpoints
function bc()
   -- TODO
end

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

-- http://snippets.luacode.org/?p=snippets/Simple_Table_Dump_7
function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
	 if type(k) ~= 'number' then k = '"'..k..'"' end
	 s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

print("debuglib.lua - loaded with " .. _VERSION)