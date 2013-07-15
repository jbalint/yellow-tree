#include <lua.h>
#include <lauxlib.h>
#include <jni.h>
#include <jvmti.h>

#include "java_bridge.h"
#include "jni_util.h"

void new_jmethod_id(lua_State *L, jmethodID method_id)
{
  jmethodID *user_data;
  user_data = lua_newuserdata(L, sizeof(jmethodID));
  *user_data = method_id;
  lua_getfield(L, LUA_REGISTRYINDEX, "jmethod_id");
  lua_setmetatable(L, -2);
}

void new_jfield_id(lua_State *L, jfieldID field_id, jclass class)
{
  lj_field_id *user_data;
  user_data = lua_newuserdata(L, sizeof(lj_field_id));
  user_data->field_id = field_id;
  user_data->class = class;
  lua_getfield(L, LUA_REGISTRYINDEX, "jfield_id");
  lua_setmetatable(L, -2);
}

void new_jmonitor(lua_State *L, jrawMonitorID monitor, const char *name)
{
  jrawMonitorID *user_data;
  user_data = lua_newuserdata(L, sizeof(jrawMonitorID));
  *user_data = monitor;
  assert(name);
  (void)name;
  lua_getfield(L, LUA_REGISTRYINDEX, "jmonitor");
  lua_setmetatable(L, -2);
}

void new_string(lua_State *L, JNIEnv *jni, jstring string)
{
  const char *utf_chars;
  utf_chars = (*jni)->GetStringUTFChars(jni, string, NULL);
  if (utf_chars)
  {
    lua_pushstring(L, utf_chars);
    (*jni)->ReleaseStringUTFChars(jni, string, utf_chars);
    EXCEPTION_CHECK(jni);
  }
  else
  {
    lua_pushnil(L);
  }
}

void new_jobject(lua_State *L, jobject object)
{
  jobject *user_data;
  if (!object)
  {
    lua_pushnil(L);
    return;
  }
  user_data = lua_newuserdata(L, sizeof(jobject));
  *user_data = object;
  lua_getfield(L, LUA_REGISTRYINDEX, "jobject");
  lua_setmetatable(L, -2);
}
