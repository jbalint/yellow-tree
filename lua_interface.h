#ifndef LUA_INTERFACE_H_
#define LUA_INTERFACE_H_

#include <lua.h>
#include <lauxlib.h>

#include <jvmti.h>

void lua_interface_init(jvmtiEnv *jvmti, jrawMonitorID mon);
void lua_command_loop(JNIEnv *jni);

#endif /* LUA_INTERFACE_H_ */
