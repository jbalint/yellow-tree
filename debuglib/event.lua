Event = {}
Event.TYPE_COMMAND = "COMMAND"
Event.TYPE_RESUME = "RESUME"

function Event.new(thread, type, data)
   local event = {thread=thread, type=type, data=data}
   return event
end

return Event
