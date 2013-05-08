#ifndef LUA_JAVA_H_
#define LUA_JAVA_H_

#include <jvmti.h>
#include <jni.h>

#include <lua.h>
#include <lauxlib.h>

#include <stdarg.h>

void lj_init(lua_State *L, JavaVM *jvm, jvmtiEnv *jvmti);
void lj_print_message(const char *format, ...);
void new_jmethod_id(lua_State *L, jmethodID method_id);
void new_jfield_id(lua_State *L, jfieldID field_id, jclass class);
void new_jobject(lua_State *L, jobject object);
void new_jmonitor(lua_State *L, jrawMonitorID monitor, const char *name);

#endif /* LUA_JAVA_H_ */
