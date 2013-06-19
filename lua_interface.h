#ifndef LUA_INTERFACE_H_
#define LUA_INTERFACE_H_

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <jvmti.h>

void lua_interface_init(JavaVM *jvm, jvmtiEnv *jvmti, jrawMonitorID mon);
void lua_start_cmd(const char *opts);
void lua_start_evp();
int lua_interface_error(lua_State *L, const char *format, ...);

#endif /* LUA_INTERFACE_H_ */
