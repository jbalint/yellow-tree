local Frame = {}

function Frame.get_frame(thread, depth)
   local frame = lj_get_stack_frame(thread.jthread, depth)
   if not frame then
	  return nil
   end
   frame.thread = thread

   setmetatable(frame, Frame)
   return frame
end

function Frame:__index(k)
   local local_var = self.method_id.local_variable_table[k]
   if local_var then
      return lj_get_local_variable(self.depth, local_var.slot, local_var.sig)
   else
	  return nil
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

return Frame
