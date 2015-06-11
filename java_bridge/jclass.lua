local jclass = { classname = "jclass" }

-- ============================================================
-- create a new jclass.
function jclass.create(object_raw)
   assert(object_raw)
   -- name is a transient and cached property in java.lang.Class, don't access it directly
   -- c.f. Class.java source
   local getNameMethod_raw = lj_get_method_id(lj_find_class("java/lang/Class"),
											  "getName", "", "Ljava/lang/String;")
   local class_name = lj_call_method(object_raw, getNameMethod_raw, false, "STR", 0)

   -- return static java.lang.Class instance
   if class_name == "java.lang.Class" then
	  return jclass.java_lang_Class_instance
   end

   -- TODO document use of global ref here
   local self = jobject.create(object_raw):global_ref() -- call superclass ctor
   jclass.init_internal(self, class_name)
   return self
end

-- ============================================================
function jclass:__index(key)
   if rawget(jclass, key) then
	  return rawget(jclass, key)
   end
   if rawget(self, key) then
	  return rawget(self, key)
   end

   local field_id = self:find_field(key)
   if field_id then
	  return field_id:get_value(self.object_raw)
   end

   -- check for any methods named `key'
   local methods = self:find_methods(key)
   if #methods > 0 then
      return jcallable_method.create(self, methods)
   end
   return rawget(self, key) or rawget(jclass, key) or jobject.__index(self, key)
end

-- ============================================================
function jclass:__tostring()
   return string.format("jclass@%s %s",
						lj_pointer_to_string(self.object_raw),
						self.name)
end

-- ============================================================
function jclass:dump(prefix)
   prefix = prefix or ""

   -- create the list of (sorted) fields as a string to print
   local fields_string = ""
   for i, f in ipairs(self.fields_array) do
	  fields_string = fields_string .. string.format("%s    %s\n", prefix, f)
   end
   if #self.fields_array > 0 then
	  fields_string = string.format("%sfields:\n%s", prefix, fields_string)
   end

   local methods_string = ""
   for i, m in ipairs(self.methods_array) do
	  methods_string = methods_string .. string.format("%s    %s\n", prefix, m)
   end
   if #self.methods_array > 0 then
	  methods_string = string.format("%smethods:\n%s", prefix, methods_string)
   end

   return string.format("%sjclass: %s\n%s%s", prefix, self.name,
						fields_string, methods_string)
end

-- ============================================================
function jclass:__gc()
   lj_delete_global_ref(self.object_raw)
end

-- ============================================================
function jclass:__eq(other)
   return getmetatable(self) == getmetatable(other) and
	  self.name == other.name
end

-- ============================================================
function jclass:init_internal(class_name)
   self.name = class_name
   self.internal_name = self.name:gsub("%.", "/")
   if class_name ~= "java.lang.Class" then
	  self.sourcefile = lj_get_source_filename(self.object_raw)
   end

   self.fields = {}
   self.fields_array = {}
   for idx, field_raw in pairs(lj_get_class_fields(self.object_raw)) do
	  local field = jfield_id.create(field_raw, self)
	  self.fields[field.name] = field
	  table.insert(self.fields_array, field)
   end
   table.sort(self.fields_array)

   self.methods = {}
   self.methods_array = {}
   for idx, method_raw in pairs(lj_get_class_methods(self.object_raw)) do
	  local method = jmethod_id.create(method_raw, self)
	  self.methods[method.name .. method.sig] = method
	  table.insert(self.methods_array, method)
   end
   table.sort(self.methods_array)

   setmetatable(self, jclass)
   return self
end

-- ============================================================
function jclass.find(class_name)
   local class_raw = lj_find_class(class_name)
   if class_raw == nil then
	  return nil
   end
   return jclass.create(class_raw)
end

-- ============================================================
function jclass:isAssignableFrom(class)
   local isAssignableFromMethod_raw = lj_get_method_id(lj_find_class("java/lang/Class"), "isAssignableFrom", "Ljava/lang/Class;", "Z")
   return lj_call_method(self.object_raw, isAssignableFromMethod_raw, false, "Z", 1, "Ljava/lang/Class;", class.object_raw)
end

-- ============================================================
-- search up the class hierarchy for methods called `search_name'
function jclass:find_methods(search_name)
   local methods = {}
   local superclass_method_id_raw = lj_get_method_id(lj_find_class("java/lang/Class"),
													 "getSuperclass", "", "Ljava/lang/Class;")
   local class = self.object_raw
   while class do
      for idx, method_id_raw in pairs(lj_get_class_methods(class)) do
		 -- match literal method names or "new" for constructors
		 local name = lj_get_method_name(method_id_raw).name
		 if name == search_name or (name == "<init>" and search_name == "new") then
			table.insert(methods, jmethod_id.create(method_id_raw, self))
		 end
      end
      class = lj_call_method(class, superclass_method_id_raw, false, "L", 0)
   end
   return methods
end

-- ============================================================
-- search up the class hierarchy for the first field called `search_name'
function jclass:find_field(search_name)
   local superclass_method_id_raw = lj_get_method_id(lj_find_class("java/lang/Class"),
													 "getSuperclass", "", "Ljava/lang/Class;")
   local class = self.object_raw
   while class do
      for idx, field_id_raw in pairs(lj_get_class_fields(class)) do
		 if lj_get_field_name(field_id_raw).name == search_name then
			return jfield_id.create(field_id_raw, self)
		 end
      end
      class = lj_call_method(class, superclass_method_id_raw, false, "L", 0)
   end
   return nil
end

-- bootstrap the lowest-level class
jclass.java_lang_Class_instance = {}
jclass.java_lang_Class_instance.object_raw = lj_new_global_ref(lj_find_class("java/lang/Class"))
jclass.java_lang_Class_instance.class = jclass.java_lang_Class_instance
setmetatable(jclass.java_lang_Class_instance, jclass)
jclass.java_lang_Class_instance:init_internal("java.lang.Class")

return jclass
