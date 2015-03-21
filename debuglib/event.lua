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

return Event
