local jobject = { classname = "jobject" }

-- ============================================================
function jobject.create(jobject_raw)
   local self = {}
   self.jobject_raw = jobject_raw
   local getClassMethod_raw = lj_get_method_id(lj_find_class("java/lang/Object"),
											   "getClass", "", "Ljava/lang/Class;")
   self.class = jclass.create(lj_call_method(self.jobject_raw, getClassMethod_raw, false, "L", 0))
   setmetatable(self, jobject)
   return self
end

-- ============================================================
function jobject:__tostring()
   return string.format("jobject@%s: %s",
						lj_pointer_to_string(self.jobject_raw),
						lj_toString(self.jobject_raw))
end

-- ============================================================
function jobject.__eq(o1, o2)
   local o1c, o2c = o1.class.name, o2.class.name
   if o1c ~= o2c then return false end

   -- String comparisons
   if o1c == "java.lang.String" then
      return lj_toString(o1.jobject_raw) == lj_toString(o2.jobject_raw)
   end

   -- TODO reference comparison? possible? useful? equals?
   return false
end

-- ============================================================
-- ALWAYS RETURNS A JOBJECT INSTANCE, has to be overridden for other classes
function jobject:global_ref()
   return jobject.create(lj_new_global_ref(self.jobject_raw))
end

-- ============================================================
function jobject:__index(key)
   if rawget(jclass, key) then
	  return rawget(jclass, key)
   end

   -- check if we have a static field by the name `key' and return the value if so
   local class = self.class
   local jfield_id = class:find_field(key)
   if jfield_id then
      return jfield_id:get_value(self.jobject_raw)
   end

   -- check for any methods named `key'
   local methods = self.class:find_methods(key)
   if #methods > 0 then
      return jcallable_method.create(self, methods)
   end

   return rawget(jobject, key)
end

return jobject
