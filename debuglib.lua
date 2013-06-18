-- debuglib.lua                              -*- indent-tabs-mode: nil -*-
-- Commands implementing the debugger
-- Loaded automatically at debugger start

--   _____                                          _     
--  / ____|                                        | |    
-- | |     ___  _ __ ___  _ __ ___   __ _ _ __   __| |___ 
-- | |    / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` / __|
-- | |___| (_) | | | | | | | | | | | (_| | | | | (_| \__ \
--  \_____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_|___/

require("java_bridge")
require("event_queue")

-- options
options = {}

-- current stack depth
depth = 1

-- breakpoint list
breakpoints = {}

-- thread condition variable -- initialized in C code
thread_resume_monitor = nil

-- next line tracking
next_line_location = nil
next_line_method_id = nil

-- i/o
dbgio = require("console_io")

-- lock to prevent multiple threads from entering the debugger simultaneously
-- TODO should be moved up into C code to protect lua_State
-- TODO doesn't prevent re-entry on same thread, will require at least g() to be called twice
-- TODO make sure this doesn't deadlock on re-entry
debug_lock = lj_create_raw_monitor("debug_lock")
-- current thread being debugged, should be set when debug_lock is acquired
-- and cleared when debug_lock is released
debug_thread = nil

-- used to wait for a debug event
debug_event = lj_create_raw_monitor("debug_event")

ThreadName = {}
ThreadName.CMD_THREAD = "this is init'd in start_cmd"

Thread = {}
Thread.__index = Thread

function Thread.new(jthread)
   local self = setmetatable({}, Thread)
   self.jthread = jthread
   self.name = jthread.getName().toString()
   self.event_queue = EventQueue.new(self.name)
   return self
end

function Thread:handle_events()
   while true do
	  local event = self.event_queue:pop()
	  --dbgio:debug("Thread ", self.name, " received event ", dump(event))
	  if event.type == EventType.RESUME then
		 return
	  elseif event.type == EventType.COMMAND then
		 -- TODO copied from start_cmd()
		 local success, m2 = pcall(event.data.chunk)
		 if not success then
			dbgio:print("Error: " .. m2)
		 elseif m2 then
			dbgio:print(m2)
		 end
	  end
   end
end

threads = {}

function current_thread()
   local jthread = java.lang.Thread.currentThread()
   local name = jthread.getName().toString()
   -- return thread object if already created
   -- TODO track and remove with thread destroy event
   if threads[name] then
	  return threads[name]
   end
   local thread = Thread.new(jthread:global_ref())
   threads[name] = thread
   dbgio:print("New Java thread: ", jthread)
   return thread
end

-- ============================================================
-- Starts command interpreter
-- ============================================================
function start_cmd()
   ThreadName.CMD_THREAD = current_thread().name
   local x = function (err)
      dbgio:write("Error: ")
      dbgio:print(debug.traceback(err, 2))
   end
   if options.runfile then
      local success, m2 = xpcall(loadfile(options.runfile), x)
      if success and not m2 then
         return true -- prevert restarting of start_cmd()
      end
   end
   -- command loop
   while true do
      dbgio:write("yt> ")
	  local cmd = dbgio:read_line() or ""
      if cmd:sub(1, 1) == "=" then
		 cmd = "return " .. cmd:sub(2)
      end
      local chunk = load(cmd)
	  if debug_thread == nil then
		 local success, m2 = pcall(chunk)
		 if not success then
			dbgio:print("Error: " .. m2)
		 elseif m2 then
			dbgio:print(m2)
		 end
	  else
		 local event = Event:new(threads[ThreadName.CMD_THREAD], EventType.COMMAND, {chunk=chunk})
		 debug_thread.event_queue:push(event)
	  end
   end
   dbgio:command_loop()
end

-- ============================================================
-- Continue execution
-- ============================================================
function g()
   if debug_thread ~= nil then
	  local event = Event:new(debug_thread, EventType.RESUME)
	  debug_thread.event_queue:push(event)
   else
	  thread_resume_monitor:notify_without_lock()
   end
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
stack_mt = {}
function stack_mt:__tostring()
   if table.maxn(self) == 0 then
      return("No code running")
   end

   local disp = ""
   for i, f in ipairs(self) do
      -- TODO limit frame count to prevent printing unreasonably large stacks
      if depth == f.depth then
         disp = disp .. "*"
      end
      disp = string.format("%s%s\n", disp, f)
   end
   return(disp)
end
function stack()
   local stack = {}
   setmetatable(stack, stack_mt)
   local frame_count = lj_get_frame_count()

   for i = 1, frame_count do
      table.insert(stack, frame_get(i))
   end
   return stack
end

-- ============================================================
-- Print local variables in current stack frame
-- ============================================================
function locals()
   local frame = frame_get(depth)
   local var_table = frame.method_id.local_variable_table
   for k, v in pairs(var_table) do
      dbgio:print(string.format("%10s = %s", k,
                                lj_get_local_variable(depth, v.slot, v.sig)))
   end
end

-- ============================================================
-- Move to the next executing line of the program
-- Possible scenarios:
--    1. the next line of code is encountered after one or
--       more single step events
--       a. This may involve a method call and a potentially
--          very large number of single step events
--    2. we are at the end of the method and continue to the
--       method of the preceding stack frame
-- ============================================================
-- temporarily renamed from next() to next_line(),
-- next() conflicts with lua interator function
function next_line(num)
   num = num or 1

   -- find location of next line
   local f = lj_get_stack_frame(depth)
   local line_nums = f.method_id.line_number_table
   for idx, ln in ipairs(line_nums) do
      if f.location < ln.location then
         next_line_location = ln.location
         next_line_method_id = f.method_id
         lj_set_jvmti_callback("single_step", cb_single_step)
         lj_set_jvmti_callback("method_exit", cb_method_exit)
         g()
         return
      end
   end

   if not next_line_location then
      lj_set_jvmti_callback("method_exit", cb_method_exit)
      g()
      return
   end
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

   local f = frame_get(num)
   if not f then
      dbgio:print("Invalid frame")
   end

   depth = num
   return f
end

function frame_get(num)
   return Frame:new(nil, num)
end

-- ============================================================
-- Add a new breakpoint
-- takes a method declaration, line number (can be 0)
-- ============================================================
function bp(method, line_num)
   local b = {}
   b.line_num = line_num or 0

   if type(method) == "string" then
      local method_spec = method_decl_parse(method)
      b.method_id = lj_get_method_id(method_spec.class, method_spec.method, method_spec.args, method_spec.ret)
      if not b.method_id then
         error("Method not found")
      end
   elseif type(method) == "userdata" then
      b.method_id = method
   else
      error("Invalid method, must be method declaration of form \"pkg/Class.name()V\" or a method_id object")
   end

   b.location = method_location_for_line_num(b.method_id, b.line_num)

   -- make sure bp doesn't already exist
   for idx, bp in ipairs(breakpoints) do
      if bp.method_id == b.method_id and bp.location == b.location then
         dbgio:print("Breakpoint already exists")
         return
      end
   end

   -- add tostring()
   setmetatable(b, b)
   b.__tostring = function(bp)
      local disp = string.format("%s.%s%s",
                                 bp.method_id.class.name,
                                 bp.method_id.name,
                                 bp.method_id.sig)
      if (bp.line_num) then
         disp = disp .. " (line " .. bp.line_num .. ")"
      end
      return disp
   end

   lj_set_breakpoint(b.method_id, b.location)
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
   -- clear all
   if not num then
      if #breakpoints == 0 then
         dbgio:print("No breakpoints")
      end
      for i = 1, #breakpoints do
         bc(1)
      end
      return
   end

   local b = breakpoints[num]
   if not b then
      dbgio:print("unknown breakpoint")
      return
   end
   local desc = string.format("%s", b)
   lj_clear_breakpoint(b.method_id, b.location)
   table.remove(breakpoints, num)
   dbgio:print("cleared ", desc)
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
   debug_lock:lock()
   debug_thread = current_thread()

   local bp
   for idx, v in pairs(breakpoints) do
      -- TODO and test location
      if v.method_id == method_id then
         bp = v
      end
   end
   assert(bp)

   depth = 1
   dbgio:print()
   dbgio:print(frame_get(1))
   local need_to_handle_events = true
   -- run handler if present and resume thread if requested
   if bp.handler then
	  local x = function (err)
		 dbgio:print(debug.traceback("Error during bp.handler: " .. err, 2))
	  end
	  local success, m2 = xpcall(bp.handler, x, bp, debug_thread)
	  -- return true means we resume the thread
	  if success and m2 then
		 need_to_handle_events = false
	  end
   end
   
   if need_to_handle_events then
	  debug_event:broadcast_without_lock()
	  debug_thread:handle_events()
   end

   debug_thread = nil
   debug_lock:unlock()
end

function cb_method_entry(thread, method_id)
end

function cb_method_exit(thread, method_id, was_popped_by_exception, return_value)
   debug_lock:lock()
   debug_thread = current_thread()

   dbgio:print(frame_get(depth))
   debug_event:broadcast_without_lock()
   debug_thread:handle_events()

   debug_thread = nil
   debug_lock:unlock()
end

function cb_single_step(thread, method_id, location)
   debug_lock:lock()
   debug_thread = current_thread()

   if next_line_method_id ~= method_id then
      -- single stepped into a different method, disable single steps until
      -- it exits
      lj_clear_jvmti_callback("single_step")
      local previous_frame_count = lj_get_frame_count()
      local check_nested_method_return = function(thread, method_id, was_popped_by_exception, return_value)
         if lj_get_frame_count() == previous_frame_count then
            lj_set_jvmti_callback("method_exit", cb_method_exit)
            lj_set_jvmti_callback("single_step", cb_single_step)
         end
      end
      lj_set_jvmti_callback("method_exit", check_nested_method_return)
   end

   if next_line_location and location >= next_line_location and next_line_method_id == method_id then
      local data = {method_id=method_id, location=location}
      lj_clear_jvmti_callback("single_step")
      lj_clear_jvmti_callback("method_exit")
      next_line_location = nil
      next_line_method_id = nil
      dbgio:print(frame_get(depth))
   	  debug_event:broadcast_without_lock()
	  debug_thread:handle_events()
   end

   debug_thread = nil
   debug_lock:unlock()
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

Frame = {}
function Frame:new(thread, depth)
   thread = thread or lj_get_current_thread()
   local frame = lj_get_stack_frame(depth, thread)
   frame.thread = thread

   setmetatable(frame, self)
   return frame
end
function Frame:__index(k)
   local local_var = self.method_id.local_variable_table[k]
   if local_var then
      return lj_get_local_variable(self.depth, local_var.slot, local_var.sig)
   end
end
function Frame:locals()
   local locals = {}
   for k, v in pairs(self.frame.method_id.local_variable_table) do
      locals[k] = self[k]
   end
   return locals
end
function Frame:__tostring()
   -- look up line number
   local line_num = -1
   for idx, ln in ipairs(self.method_id.line_number_table or {}) do
      if self.location >= ln.location then
         line_num = ln.line_num
      else
         break
      end
   end

   return string.format("%6s %s.%s%s - %s (%s:%s)",
                        "[" .. self.depth .. "]",
                        self.method_id.class.name,
                        self.method_id.name,
                        self.method_id.sig,
                        self.location,
                        self.method_id.class.sourcefile or "<unknown>",
                        line_num)
end

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
-- "temporary" table to facilitate addressing Java classes
-- in the form pkg.x.Class directly in Lua
function fq_class_search(pkg, previous_t)
   local t = {}
   t.pkg = pkg
   if previous_t then
      t.pkg = previous_t.pkg .. "." .. t.pkg
   end

   local mt = getmetatable(t) or (setmetatable(t, {}) and getmetatable(t))
   mt.__index = function(t, k)
      local class = lj_find_class(string.gsub(t.pkg .. "." .. k, "[.]", "/"))
      if class then
         -- we found a class
         return class
      else
         -- search another level of packages
         return fq_class_search(k, t)
      end
   end
   mt.__tostring = function(t)
      return "fully qualified class search, pkg=" .. t.pkg
   end
   return t
end

-- ============================================================
-- Find a local variable
-- Return "value, k" if found, "nil, nil" otherwise
function get_local_variable(k)
   local frame = lj_get_stack_frame(depth)
   if not frame then
      return nil, nil
   end
   local locals = frame.method_id.local_variable_table
   if not locals then
      return nil, nil
   end
   local l = locals[k]
   if l then
      return lj_get_local_variable(depth, l.slot, l.sig), k
   end
end

-- ============================================================
-- make locals, Java classes, etc available throughout
function init_locals_environment()
   local java_pkg_tlds = {"java", "javax", "com", "org", "net", "testsuite"}
   local mt = getmetatable(_ENV) or (setmetatable(_ENV, {}) and getmetatable(_ENV))
   mt.__index = function(t, k)
	  if not k then return nil end

      -- find a local variable
      local lv, name = get_local_variable(k)
      if name then return lv end

      -- TODO: try members of `this'

      -- find a fully-qualified class
      local class = lj_find_class(k)
      if class then return class end

      -- TODO: try class name without package

      -- last possibility:
      -- start com.*.Class search
      for idx, tld in ipairs(java_pkg_tlds) do
         if k == tld then
            return fq_class_search(k, nil)
         end
      end
   end
   mt.__newindex = function(t, k, v)
      rawset(t, k, v)
   end
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
-- Find the location (offset) in a method for a given line number
function method_location_for_line_num(method_id, line_num)
   if not line_num then return -1 end
   local lnt = method_id.line_number_table
   if not lnt then return -1 end
   for idx, ln in ipairs(lnt) do
      if line_num == ln.line_num then
         return ln.location
      elseif line_num < ln.line_num and idx > 1 then
         return lnt[idx-1].location
      elseif line_num < ln.line_num then
         return 0
      end
   end
   return lnt[#lnt].location
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
   -- print(this.polyMorphic1())
   -- print(this.polyMorphic2(4))
   -- print(this.polyMorphic3(19283))
   -- print(this.polyMorphic3("99"))
   -- print(this.polyMorphic4(12, this))
   -- print(this.polyMorphic5(17, this))
end

function run_tests()
   loadfile("test/test_basic.lua")()
   loadfile("test/test_lua_java.lua")()
   loadfile("test/test_java_bridge.lua")()
end

function y()
   local threads = lj_get_all_threads()
   local main_thread
   for i, t in ipairs(threads) do
      if t.getName().toString() == "main" then
         main_thread = t
      end
   end
   print(dump(main_thread))
   print(dump(main_thread.frame_count))
   print(dump(main_thread.frames))
end
