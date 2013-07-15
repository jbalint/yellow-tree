local Frame = require("debuglib/frame")

-- local EventQueue = require("event_queue")
require("event_queue")

local jthread = {
   classname = "jthread",
   __eq = jobject.__eq
}

-- ============================================================
function jthread.create(jobject_raw)
   local self = jobject.create(jobject_raw):global_ref() -- call superclass ctor
   setmetatable(self, jthread)
   self.event_queue = EventQueue.new(self.name)
   return self
end

-- ============================================================
function jthread:get_raw_frame(depth)
   return lj_get_stack_frame(self.jobject_raw, depth)
end

-- ============================================================
function jthread:__gc()
   lj_delete_global_ref(self.jobject_raw)
end

-- ============================================================
function jthread:__index(key)
   if key == "frame_count" then
	  return lj_get_frame_count(self.jobject_raw)
   elseif key == "name" then
	  return self.getName().toString()
   end
   return rawget(self, key) or rawget(jthread, key) or jobject.__index(self, key)
end

-- ============================================================
function jthread:__tostring()
   return string.format("jthread@%s: %s",
						lj_pointer_to_string(self.jobject_raw),
						lj_toString(self.jobject_raw))
end

-- ============================================================
function jthread:handle_events()
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

return jthread
