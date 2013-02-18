#ifndef LUA_JAVA_H_
#define LUA_JAVA_H_

#include <jvmti.h>
#include <jni.h>

#include <lua.h>
#include <lauxlib.h>

void lj_init(lua_State *L);
int lj_get_frame_count(lua_State *L, jvmtiEnv *jvmti);

#endif /* LUA_JAVA_H_ */
