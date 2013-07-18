local jcallable_method = { classname = "jcallable_method" }
jcallable_method.__index = jcallable_method

-- ============================================================
-- parse an arg spec from a method signature into individual components
local function parse_arg_spec(argspec)
   if argspec == "" then return {} end
   local argarray = {}
   while argspec ~= "" do
      local char1 = string.sub(argspec, 1, 1)
      local char2 = string.sub(argspec, 2, 2)
      if char1 == "L" or (char1 == "[" and char2 == "L") then
		 -- object / object array
		 local endi = string.find(argspec, ";")
		 table.insert(argarray, string.sub(argspec, 1, endi))
		 argspec = string.sub(argspec, endi + 1)
      elseif char1 == "[" then
		 -- primitive array
		 table.insert(argarray, char1 .. char2)
		 argspec = string.sub(argspec, 3)
      else
		 -- primitive type (single char)
		 table.insert(argarray, char1)
		 argspec = string.sub(argspec, 2)
      end
   end
   return argarray
end

-- ============================================================
function jcallable_method.create(object, possible_methods)
   local self = {}
   self.object = object
   self.possible_methods = possible_methods
   setmetatable(self, jcallable_method)
   return self
end

-- ============================================================
-- perform the actual method call. this will match the `args'
-- to one of the `possible_methods'
function jcallable_method:__call(...)
   local args = {...}
   local argc = #args
   -- filter out non-matching methods
   local possible2 = {}
   local object = self.object
   --.... this loop only exists because there is no "continue" in lua
   for i,m in ipairs(self.possible_methods) do
      -- short circuit match for no args
      if #args == 0 and m.args == "" then
		 if m.modifiers.static and object.class.name ~= "java.lang.Class" then
			object = object.class
		 end
		 return m(object.object_raw, 0)
      end

      -- only try to match methods with the same number of arguments
      if #parse_arg_spec(m.args) == argc then
		 table.insert(possible2, m)
      end
   end

   -- try to match types with args
   local jargs = {}
   for i, m in ipairs(possible2) do
      local atypes = parse_arg_spec(m.args)
      for i, t in ipairs(atypes) do
		 local argi = args[i]
		 if argi == nil then
			table.insert(jargs, "V")
			table.insert(jargs, nil)
			-- TODO redo this SOON
		 --elseif type(argi) == "number" then
			-- handle numeric arguments
		 elseif (t == "Z") and type(argi) == "boolean" then
			table.insert(jargs, t)
			table.insert(jargs, argi)
		 elseif (t == "I" or t == "J") and type(argi) == "number" then
			table.insert(jargs, t)
			table.insert(jargs, math.floor(argi))
		 elseif (t == "F" or t == "D") and type(argi) == "number" then
			table.insert(jargs, t)
			table.insert(jargs, argi)
		 elseif string.sub(t, 1, 1) == "[" and type(argi) == "table" and argi.object_raw and argi.class.name == t then
			table.insert(jargs, "[")
			table.insert(jargs, argi.object_raw)
		 elseif t == "Ljava/lang/String;" and type(argi) ~= "userdata" and type(argi) ~= "table" then
			table.insert(jargs, "STR")
			table.insert(jargs, string.format("%s", argi))
		 elseif (string.sub(t, 1, 1) == "L" or string.sub(t, 1, 2) == "[L") and
		        type(argi) == "table" and
			    argi.object_raw then
		        --string.find(string.format("%s", argi), "jobject@") == 1 then
			local name = t
			-- from L; for non-arrays
			if string.sub(t, 1, 1) == "L" then
			   name = string.sub(t, 2, #t - 1)
			end
			local tc = jclass.find(name)
			assert(tc)
			if tc:isAssignableFrom(argi.class) then
			   table.insert(jargs, "L")
			   table.insert(jargs, argi.object_raw)
			end
		 end

		 -- call only if all args matched
		 if #jargs == (argc * 2) then
			if m.modifiers.static and object.class.name ~= "java.lang.Class" then
			   object = object.class
			end
			return m(object.object_raw, argc, unpack(jargs))
		 end
      end
      --print("more than one possible method, using: " .. method_id.name .. " from " .. lj_toString(method_id.class))
   end
   error("No matching method for given arguments: " .. dump(self.possible_methods))
end

return jcallable_method
