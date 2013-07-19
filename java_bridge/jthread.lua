local Frame = require("debuglib/frame")
local Event = require("debuglib/event")
local EventQueue = require("debuglib/event_queue")

local jthread = {
   classname = "jthread",
   __eq = jobject.__eq
}

-- ============================================================
function jthread.create(object_raw)
   assert(object_raw)
   local self = jobject.create(object_raw):global_ref() -- call superclass ctor
   setmetatable(self, jthread)
   self.event_queue = EventQueue.new(self.name)
   self.frames = jthread.frames.new(self)
   return self
end

-- ============================================================
function jthread:__gc()
   lj_delete_global_ref(self.object_raw)
end

-- ============================================================
function jthread:__index(key)
   if key == "name" then
	  return self.getName().toString()
   end
   return rawget(self, key) or rawget(jthread, key) or jobject.__index(self, key)
end

-- ============================================================
function jthread:__tostring()
   return string.format("jthread@%s: %s",
						lj_pointer_to_string(self.object_raw),
						lj_toString(self.object_raw))
end

-- ============================================================
function jthread:handle_events()
   while true do
	  local event = self.event_queue:pop()
	  if event.type == Event.TYPE_RESUME then
		 return
	  elseif event.type == Event.TYPE_COMMAND then
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

-- ============================================================
-- inner class to allow accessing frames as an array
jthread.frames = {}
-- ============================================================

-- ============================================================
function jthread.frames.new(thread)
   assert(thread)
   local self = { thread = thread }
   setmetatable(self, jthread.frames)
   return self
end

-- ============================================================
function jthread.frames:__len()
   return lj_get_frame_count(self.thread.object_raw)
end

-- ============================================================
function jthread.frames:__index(key)
   if type(key) == "number" then
	  local frame_raw = lj_get_stack_frame(self.thread.object_raw, key)
	  if not frame_raw then
		 return nil
	  else
		 return Frame.create(frame_raw, self.thread)
	  end
   end
   return rawget(jthread.frames, key)
end

-- ============================================================
function jthread.frames:__tostring()
   local disp = ""
   -- TODO limit frame count to prevent printing unreasonably large stacks
   for i = 1, #self do
	  local f = self[i]
      if depth == f.depth then
         disp = disp .. "*"
      end
      disp = string.format("%s%s\n", disp, f)
   end
   return(disp)
end

return jthread
