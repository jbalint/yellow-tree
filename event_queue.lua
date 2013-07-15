-- event_queue.lua
-- Event queue - provides a synchronized event queue

Event = {}

EventType = {}
--EventType.BREAKPOINT = "BREAKPOINT"
EventType.COMMAND = "COMMAND"
EventType.RESUME = "RESUME"
--EventType.METHOD_ENTRY = "METHOD_ENTRY"
--EventType.METHOD_EXIT = "METHOD_EXIT"
--EventType.SINGLE_STEP = "SINGLE_STEP"

function Event:new(thread, type, data)
   local event = {thread=thread, type=type, data=data}
   return event
end

EventQueue = {}
EventQueue.__index = EventQueue

-- Create a new event queue
function EventQueue.new(name)
   local self = setmetatable({}, EventQueue)
   self.events = {}
   self.lock = jmonitor.new(name .. "_event_queue")
   return self
end

-- Get the next event, waiting if one is not immediately available
function EventQueue:pop()
   self.lock:lock()
   if table.maxn(self.events) == 0 then
      self.lock:wait()
   end
   local event = table.remove(self.events, 1)
   self.lock:unlock()
   return event
end

-- Push an event to the queue
function EventQueue:push(event)
   self.lock:lock()
   table.insert(self.events, event)
   self.lock:broadcast()
   self.lock:unlock()
end

