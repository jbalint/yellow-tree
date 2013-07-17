local jmonitor = { classname = "jmonitor" }
jmonitor.__index = jmonitor

-- ============================================================
-- create a new jmonitor object including raw pointer creation
function jmonitor.new(name)
   assert(type(name) == "string")
   local monitor_raw = lj_create_raw_monitor(name)
   local self = jmonitor.create(monitor_raw)
   self.name = name
   return self
end

-- ============================================================
-- create a new jmonitor object from a raw pointer
-- jmonitor.new() should be used where possible
function jmonitor.create(monitor_raw)
   assert(type(monitor_raw) == "userdata")
   local self = {}
   self.monitor_raw = monitor_raw
   setmetatable(self, jmonitor)
   return self
end

-- ============================================================
function jmonitor:__tostring()
   return string.format("jmonitor@%s",
						lj_pointer_to_string(self.monitor_raw))
end

-- ============================================================
function jmonitor:destroy()
   lj_destroy_raw_monitor(self.monitor_raw)
end

-- ============================================================
function jmonitor:lock()
   lj_raw_monitor_enter(self.monitor_raw)
end

-- ============================================================
function jmonitor:unlock()
   lj_raw_monitor_exit(self.monitor_raw)
end

-- ============================================================
function jmonitor:wait(time)
   lj_raw_monitor_wait(self.monitor_raw, time or 0)
end

-- ============================================================
function jmonitor:notify()
   lj_raw_monitor_notify(self.monitor_raw)
end

-- ============================================================
function jmonitor:broadcast()
   lj_raw_monitor_notify_all(self.monitor_raw)
end

-- ============================================================
function jmonitor:wait_without_lock(time)
   self:lock()
   self:wait(time)
   self:unlock()
end

-- ============================================================
function jmonitor:notify_without_lock()
   self:lock()
   self:notify()
   self:unlock()
end

-- ============================================================
function jmonitor:broadcast_without_lock()
   self:lock()
   self:broadcast()
   self:unlock()
end

return jmonitor
