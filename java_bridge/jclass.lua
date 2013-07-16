local jclass = { __tostring = jobject.__tostring, classname = "jclass" }

-- ============================================================
-- create a new jclass.
function jclass.create(object_raw)
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

   local jfield_id = self:find_field(key)
   if jfield_id then
	  return jfield_id:get_value(self.object_raw)
   end

   -- check for any methods named `key'
   local methods = self:find_methods(key)
   if #methods > 0 then
      return jcallable_method.create(self, methods)
   end
   return rawget(self, key) or rawget(jclass, key) or jobject.__index(self, key)
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
   if class_name ~= "java.lang.Class" then
	  self.sourcefile = lj_get_source_filename(self.object_raw)
   end

   self.fields = {}
   for idx, field_raw in pairs(lj_get_class_fields(self.object_raw)) do
	  local field = jfield_id.create(field_raw, self)
	  self.fields[field.name] = field
   end

   self.methods = {}
   for idx, method_raw in pairs(lj_get_class_methods(self.object_raw)) do
	  local method = jmethod_id.create(method_raw, self)
	  self.methods[method.name .. method.sig] = method
   end

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
-- search up the class hierarchy for the first field called `name'
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
