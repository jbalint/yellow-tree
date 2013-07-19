local jmethod_id = { classname = "jmethod_id" }
jmethod_id.__index = jmethod_id

-- ============================================================
function jmethod_id.create(method_id_raw, class)
   assert(method_id_raw)
   assert(class)
   assert(class.classname == "jclass")
   local self = {}
   self.method_id_raw = method_id_raw
   self.class = class
   local ljmn = lj_get_method_name(self.method_id_raw)
   self.name = ljmn.name
   self.sig = ljmn.sig

   -- parse sig for args and ret
   local sig = self.sig
   sig = string.sub(sig, 2) -- remove (
   self.args = string.match(sig, "^.*%)")
   self.args = string.sub(self.args, 1, string.len(self.args) - 1)
   self.ret = string.match(sig, "%).*$")
   self.ret = string.sub(self.ret, 2)

   self.line_number_table = lj_get_line_number_table(self.method_id_raw)
   self.local_variable_table = lj_get_local_variable_table(self.method_id_raw)
   self.modifiers = lj_get_method_modifiers_table(lj_get_method_modifiers(self.method_id_raw))

   setmetatable(self, jmethod_id)
   return self
end

-- ============================================================
function jmethod_id.find(complete_method_sig)
   local method_sig = jmethod_id.parse_complete_method_signature(complete_method_sig)
   local jclass = jclass.find(method_sig.class_name)
   if not jclass then
	  return nil
   end
   -- TODO defer this to class? jclass.find_method(...)
   local raw_method_id = lj_get_method_id(jclass.object_raw, method_sig.method_name,
										  method_sig.args, method_sig.ret)
   if raw_method_id then
	  return jmethod_id.create(raw_method_id, jclass)
   else
	  return nil
   end
end

-- ============================================================
function jmethod_id.from_raw_method_id(method_id_raw)
   local class = jclass.create(lj_get_method_declaring_class(method_id_raw))
   return jmethod_id.create(method_id_raw, class)
end

-- ============================================================
-- Parse a complete method signature of the form
-- java/io/PrintStream.println(Ljava/lang/String;)V
function jmethod_id.parse_complete_method_signature(complete_method_sig)
   local chars = 0
   local md = {}

   md.class_name = string.match(complete_method_sig, "^.*%.") -- up until .
   chars = string.len(md.class_name)
   md.class_name = string.sub(md.class_name, 0, string.len(md.class_name) - 1) -- remove .

   md.method_name = string.match(complete_method_sig, "^.*%(", chars + 1) -- up until (
   chars = chars + string.len(md.method_name)
   md.method_name = string.sub(md.method_name, 0, string.len(md.method_name) - 1) -- remove (

   md.args = string.match(complete_method_sig, "^.*%)", chars + 1) -- up until )
   chars = chars + string.len(md.args)
   md.args = string.sub(md.args, 0, string.len(md.args) - 1) -- remove )

   md.ret = string.sub(complete_method_sig, chars + 1) -- rest

   return md
end

-- ============================================================
function jmethod_id:get_preferred_ret_type()
   local ret = self.ret
   -- prefer to return a native string ONLY for this method
   if self.name == "toString" then
      ret = "STR"
   elseif string.sub(ret, 1, 1) == "L" then
      ret = "L"
	    -- return an object for constructors
   elseif self.name == "<init>" then
      ret = "L"
   end
   return ret
end

-- ============================================================
function jmethod_id:__call(object_raw, argCount, ...)
   local ret_val = lj_call_method(object_raw, self.method_id_raw,
								  self.modifiers.static,
								  self:get_preferred_ret_type(),
								  argCount, ...)
   return create_return_value(ret_val, self:get_preferred_ret_type())
end

-- ============================================================
function jmethod_id.__eq(m1, m2)
   return m1.name == m2.name and
      m1.sig == m2.sig and
      m1.class == m2.class
end

-- ============================================================
function jmethod_id:__tostring()
   return string.format("jmethod_id@%s %s.%s%s",
						lj_pointer_to_string(self.method_id_raw),
						self.class.name,
						self.name,
						self.sig)
end

-- ============================================================
function jmethod_id.__lt(m1, m2)
   if m1.name ~= m2.name then
	  return m1.name < m2.name
   end

   return m1.sig < m2.sig
end

return jmethod_id
