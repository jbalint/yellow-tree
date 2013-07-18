-- debuglib.lua                              -*- indent-tabs-mode: nil -*-
-- Commands implementing the debugger
-- Loaded automatically at debugger start

--   _____                                          _     
--  / ____|                                        | |    
-- | |     ___  _ __ ___  _ __ ___   __ _ _ __   __| |___ 
-- | |    / _ \| '_ ` _ \| '_ ` _ \ / _` | '_ \ / _` / __|
-- | |___| (_) | | | | | | | | | | | (_| | | | | (_| \__ \
--  \_____\___/|_| |_| |_|_| |_| |_|\__,_|_| |_|\__,_|___/

require("java_bridge/java_bridge")
require("event_queue")
local Frame = require("debuglib/frame")

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
debug_lock = jmonitor.new("debug_lock")
-- current thread being debugged, should be set when debug_lock is acquired
-- and cleared when debug_lock is released
debug_thread = nil

-- used to wait for a debug event
debug_event = jmonitor.new("debug_event")

ThreadName = {}
ThreadName.CMD_THREAD = "this is init'd in start_cmd"

threads = {}

function current_thread()
   -- "java.lang.Thread.currentThread()" here will cause recursion with env class lookup
   local thread = jclass.find("java/lang/Thread").currentThread()
   -- return thread object if already created
   -- TODO track and remove with thread destroy event
   if threads[thread.name] then
	  return threads[thread.name]
   end
   threads[thread.name] = thread
   dbgio:print("New Java thread: ", thread)
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
   local frame_count = current_thread().frame_count

   for i = 1, frame_count do
      table.insert(stack, Frame.get_frame(current_thread(), i))
   end
   return stack
end

-- ============================================================
-- Print local variables in current stack frame
-- ============================================================
function locals()
   local frame = Frame.get_frame(current_thread(), depth)
   local var_table = frame.method_id.local_variable_table
   if var_table == nil then
      dbgio:print("No local variable table")
      return
   end
   for k, v in pairs(var_table) do
      dbgio:print(string.format("%10s = %s", k, frame[k]))
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
   local f = Frame.get_frame(current_thread(), depth)
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

   -- fallback to step out of method (from last line of method)
   lj_set_jvmti_callback("method_exit", cb_method_exit)
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

   local f = Frame.get_frame(current_thread(), num)
   if not f then
      dbgio:print("Invalid frame")
   end

   depth = num
   return f
end

-- ============================================================
-- Add a new breakpoint
-- takes a method declaration, line number (can be 0)
-- ============================================================
function bp(method, line_num)
   local b = {}
   b.line_num = line_num or 0

   if type(method) == "string" then
      b.method_id = jmethod_id.find(method)
      if not b.method_id then
         error("Method not found")
      end
   elseif type(method) == "userdata" then
      b.method_id = method
   else
      error("Invalid method, must be method declaration of form \"pkg/Class.name()V\" or a method_id object")
   end

   b.location = method_location_for_line_num(b.method_id, b.line_num)
   if b.location < 0 then
      b.location = 0
   end

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

   lj_set_breakpoint(b.method_id.method_id_raw, b.location)
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
function cb_breakpoint(thread_raw, method_id_raw, location)
   debug_lock:lock()
   debug_thread = current_thread()

   -- TODO should be done in C code?
   local method_id = jmethod_id.from_raw_method_id(method_id_raw)

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
   dbgio:print(Frame.get_frame(current_thread(), 1))
   local need_to_handle_events = true
   -- run handler if present and resume thread if requested
   if bp.handler then
	  local x = function (err)
		 dbgio:print(debug.traceback("Error during bp.handler: " .. err, 2))
	  end
	  local success, m2 = xpcall(bp.handler, x, bp, debug_thread)
	  -- return false/nil (no return) means we resume the thread
	  if success and not m2 then
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

function cb_method_entry(thread_raw, method_id_raw)
end

function cb_method_exit(thread_raw, method_id_raw, was_popped_by_exception, return_value)
   debug_lock:lock()
   debug_thread = current_thread()

   dbgio:print(Frame.get_frame(current_thread(), depth))
   debug_event:broadcast_without_lock()
   debug_thread:handle_events()

   debug_thread = nil
   debug_lock:unlock()
end

function cb_single_step(thread_raw, method_id_raw, location)
   debug_lock:lock()
   debug_thread = current_thread()

   local method_id = jmethod_id.from_raw_method_id(method_id_raw)

   if next_line_method_id ~= method_id then
      -- single stepped into a different method, disable single steps until
      -- it exits
      lj_clear_jvmti_callback("single_step")
      local previous_frame_count = current_thread().frame_count
      local check_nested_method_return = function(thread_raw, method_id_raw, was_popped_by_exception, return_value)
         local thread = jthread.create(thread_raw)
         if thread.frame_count == previous_frame_count then
            lj_set_jvmti_callback("method_exit", cb_method_exit)
            lj_set_jvmti_callback("single_step", cb_single_step)
         end
      end
      lj_set_jvmti_callback("method_exit", check_nested_method_return)
   elseif next_line_location and location >= next_line_location and next_line_method_id == method_id then
      local data = {method_id=method_id, location=location}
      lj_clear_jvmti_callback("method_exit")
      lj_clear_jvmti_callback("single_step")
      next_line_location = nil
      next_line_method_id = nil
      dbgio:print(Frame.get_frame(current_thread(), depth))
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
      local class = jclass.find(string.gsub(t.pkg .. "." .. k, "[.]", "/"))
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
   local frame = Frame.get_frame(current_thread(), depth)
   if not frame or
      not frame.method_id.local_variable_table or
      not frame.method_id.local_variable_table[k] then
      return nil, nil
   end
   return frame[k], k
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
      local class = jclass.find(k)
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
-- Find the location (offset) in a method for a given line number
function method_location_for_line_num(jmethod_id, line_num)
   if not line_num then return -1 end
   local lnt = jmethod_id.line_number_table
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
   dump_params = {}
   dump_depth = 0
   return dump_internal(o)
end

function dump_internal(o)
   if type(o) == "table" and dump_depth > 1 then
      return "<table>"
   end
   dump_depth = dump_depth + 1
   if type(o) == "table" and dump_params[o] ~= nil then
      dump_depth = dump_depth - 1
      return "<recursion>"
   end
   dump_params[o] = 1
   local prefix = ""
   for i = 1, dump_depth do
      prefix = prefix .. "  "
   end

   local classname = ""
   if type(o) == "table" and o.classname then
      classname = o.classname
   end

   if classname == "jclass" then
      return o.dump(prefix)
   elseif classname == "jfield_id" or classname == "jmethod_id" then
      return string.format("%s%s", prefix, o)
   elseif type(o) == 'table' then
      local s = '{ ' .. "\n"
      for k,v in pairs(o) do
         if type(k) == 'table' then
            k = dump_internal(k)
         elseif type(k) ~= 'number' then
            k = '"'..k..'"'
         end
         if type(v) == "DISABLEtable" then
            s = s .. '['..k..'] = ' .. "<table>" .. ','
         else
            s = s .. prefix .. '['..k..'] = ' .. dump_internal(v) .. ','
         end
         s = s .. "\n"
      end
      dump_depth = dump_depth - 1
      return s .. '} '
   else
      dump_depth = dump_depth - 1
      return tostring(o)
   end
end

init_locals_environment()
init_jvmti_callbacks()
print("debuglib.lua - loaded with " .. _VERSION)
