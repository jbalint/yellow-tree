local Frame = { classname = "Frame" }

--- Create a new stack frame object
-- @param frame_raw A "raw" stack frame (from lj_get_stack_frame())
-- This partially initialized object is used as the instance. It
-- contains location, method_id_raw, and depth
-- @param thread The thread this frame is executing on
function Frame.create(frame_raw, thread)
   assert(type(frame_raw) == "table")
   assert(thread)
   self = frame_raw
   self.method_id = jmethod_id.from_raw_method_id(self.method_id_raw)
   self.thread = thread
   setmetatable(self, Frame)
   return self
end

-- ============================================================
function Frame:__index(k)
   -- check for local variables on the frame
   if self.method_id.local_variable_table then
	  local local_var = self.method_id.local_variable_table[k]
	  if local_var then
		 local ret_val = lj_get_local_variable(self.depth, local_var.slot, local_var.sig)
		 return create_return_value(ret_val, local_var.sig)
	  end
   end
   return rawget(Frame, k)
end

-- ============================================================
function Frame:locals()
   local locals = {}
   for k, v in pairs(self.frame.method_id.local_variable_table) do
      locals[k] = self[k]
   end
   return locals
end

function Frame:local_slot(slot, sig)
   return create_return_value(lj_get_local_variable(self.depth, slot, sig), sig)
end

-- ============================================================
function Frame:force_return(ret_val)
   if self.depth ~= 1 then
	  error("Only the innermost stack frame can be forced to return")
   end

   if self.method_id.ret == "V" then
	  lj_force_early_return_void(self.thread.object_raw)
   elseif self.method_id.ret:sub(-1) == ";" and ret_val and jobject.is_jobject(ret_val) then
	  lj_force_early_return_object(self.thread.object_raw, ret_val.object_raw)
   -- TODO coersion to java objects should be re-usable
	  -- TODO additional coersion here
   elseif self.method_id.ret == "Ljava/lang/String;" then
	  lj_force_early_return_object(self.thread.object_raw,
								   java.lang.String.new(tostring(ret_val)).object_raw)
   else -- TODO handle primitive return types
	  error("Unhandled case")
   end
end

-- ============================================================
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
