local jarray = {
   classname = "jarray",
   __tostring = jobject.__string,
   __eq = jobject.__eq
}

-- ============================================================
function jarray.create(object_raw)
   assert(object_raw)
   local self = jobject.create(object_raw) -- call superclass ctor
   setmetatable(self, jarray)
   self.component_type = self.class.internal_name:sub(2)
   return self
end

-- ============================================================
function jarray:__len()
   return lj_get_array_length(self.object_raw)
end

-- ============================================================
function jarray:__index(key)
   if key == "length" then
	  return self:__len()
   elseif type(key) == "number" then
	  local ret_val = lj_get_array_element(self.object_raw, self.class.name, key)
	  -- sub() to remove the [, so we have the "element" type
	  return create_return_value(ret_val, self.class.name:sub(2))
   end
   return rawget(self, key) or rawget(jarray, key) or jobject.__index(self, key)
end

-- ============================================================
function jarray:__newindex(key, value)
   if type(key) == "number" then
	  if value ~= nil and self.component_type:sub(-1) == ";" then
		 value = jobject.coerce(value, self.component_type)
		 lj_set_array_element(self.object_raw, self.class.name, key, value.object_raw)
	  elseif value ~= nil then
		 -- primitive type, has to match
		 lj_set_array_element(self.object_raw, self.class.name, key, value)
	  else
		 lj_set_array_element(self.object_raw, self.class.name, key, nil)
	  end
   else
	  rawset(self, key, value)
   end
end

return jarray
