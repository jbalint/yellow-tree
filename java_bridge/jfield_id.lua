local jfield_id = {}
jfield_id.__index = jfield_id

-- ============================================================
function jfield_id.create(jfield_id_raw, class)
   assert(class)
   assert(class.classname == "jclass")
   local self = {}
   self.jfield_id_raw = jfield_id_raw
   self.class = class
   local ljfn = lj_get_field_name(self.jfield_id_raw)
   self.name = ljfn.name
   self.sig = ljfn.sig
   self.modifiers = lj_get_field_modifiers_table(lj_get_field_modifiers(self.jfield_id_raw))
   setmetatable(self, jfield_id)
   return self
end

-- ============================================================
function jfield_id:__tostring()
   return string.format("jfield_id@%s %s.%s type=%s",
						lj_pointer_to_string(self.jfield_id_raw),
						self.class.name,
						self.name,
						self.sig)
end

-- ============================================================
function jfield_id.__eq(f1, f2)
   return f1.name == f2.name and
      f1.sig == f2.sig and
      f1.class == f2.class
end

-- ============================================================
function jfield_id:get_value(jobject_raw)
   local ret_val = lj_get_field(jobject_raw, self.jfield_id_raw, self.modifiers.static)
   return create_return_value(ret_val, self.sig)
end

return jfield_id
