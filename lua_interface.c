#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "lua_interface.h"
#include "lua_java.h"

static lua_State *lua_state;

void lua_interface_init(jvmtiEnv *jvmti, jrawMonitorID mon)
{
  lua_state = luaL_newstate();
  if (lua_state == NULL)
  {
    fprintf(stderr, "Failed to initialize Lua");
    abort();
  }
  luaL_openlibs(lua_state);

  lj_init(lua_state, jvmti);
  /* this must be called AFTER lj_init() so the JVMTI callbacks can be registered */
  if (luaL_dostring(lua_state, "require('debuglib')"))
  {
    fprintf(stderr, "Failed to load debuglib.lua: %s\n", lua_tostring(lua_state, -1));
    abort();
  }
  lj_set_jvm_exec_monitor(mon);
}

void lua_start(JNIEnv *jni, const char *opts)
{
  lj_set_jni(jni);
  lua_getglobal(lua_state, "setopts"); /* from debuglib.lua */
  lua_pushstring(lua_state, opts);
  if (lua_pcall(lua_state, 1, 0, 0))
  {
    fprintf(stderr, "Error setting options: %s\n", lua_tostring(lua_state, -1));
    lua_pop(lua_state, 1);
  }
  luaL_dostring(lua_state, "start()");
}
