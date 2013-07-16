-- expose classes globally

-- ============================================================
-- metatables for raw userdata objects
debug.getregistry()["jfield_id"] = {
   __tostring = function(o)
	  return "jfield_id@" .. lj_pointer_to_string(o)
   end
}
debug.getregistry()["jmethod_id"] = {
   __tostring = function(o)
	  return "jmethod_id@" .. lj_pointer_to_string(o)
   end
}
debug.getregistry()["jmonitor"] = {}
debug.getregistry()["jobject"] = {}
--debug.getregistry()["jclass"] = {}
--debug.getregistry()["jthread"] = {}
--debug.getregistry()["jarray"] = {}

-- ============================================================
jfield_id = require("java_bridge/jfield_id")
jmethod_id = require("java_bridge/jmethod_id")
jmonitor = require("java_bridge/jmonitor")

jobject = require("java_bridge/jobject")
jarray = require("java_bridge/jarray")
jclass = require("java_bridge/jclass")
jthread = require("java_bridge/jthread")

jcallable_method = require("java_bridge/jcallable_method")

-- ============================================================
-- kind of hack-y, but easier than doing it in every possible place in C code
function create_jobject(object_raw)
   if not object_raw then
	  return nil
   end

   -- get the class name
   local getClassMethod_raw = lj_get_method_id(lj_find_class("java/lang/Object"),
											   "getClass", "", "Ljava/lang/Class;")
   local class_raw = lj_call_method(object_raw, getClassMethod_raw, false, "L", 0)
   local getNameMethod_raw = lj_get_method_id(lj_find_class("java/lang/Class"),
											  "getName", "", "Ljava/lang/String;")
   local class_name = lj_call_method(class_raw, getNameMethod_raw, false, "STR", 0)

   -- discriminate based on name
   if class_name == "java.lang.Class" then
	  return jclass.create(object_raw)
   elseif class_name == "java.lang.Thread" then
	  return jthread.create(object_raw)
   elseif class_name:sub(1, 1) == "[" then
	  return jarray.create(object_raw)
   end

   -- see if java.lang.Thread is "assignable from" the class
   local isAssignableFromMethod_raw = lj_get_method_id(lj_find_class("java/lang/Class"),
													   "isAssignableFrom", "Ljava/lang/Class;", "Z")
   if lj_call_method(lj_find_class("java/lang/Thread"),
					 isAssignableFromMethod_raw, false, "Z", 1, "Ljava/lang/Class;",
					 class_raw) then
	  return jthread.create(object_raw)
   end

   -- fallback to jobject
   return jobject.create(object_raw)
end

-- ============================================================
function create_return_value(ret_val, type)
   if type:sub(1, 1) == "L" or type:sub(1, 1) == "[" then
	  return create_jobject(ret_val)
   else
	  return ret_val
   end
end
