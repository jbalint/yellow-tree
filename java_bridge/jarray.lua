local jarray = {
   classname = "jarray",
   __tostring = jobject.__string,
   __eq = jobject.__eq
}

-- ============================================================
function jarray.create(object_raw)
   local self = jobject.create(object_raw) -- call superclass ctor
   setmetatable(self, jarray)
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

return jarray
