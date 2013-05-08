#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "lua_interface.h"
#include "lua_java.h"

static lua_State *lua_state;

void lua_interface_init(JavaVM *jvm, jvmtiEnv *jvmti, jrawMonitorID thread_resume_monitor)
{
  lua_state = luaL_newstate();
  if (lua_state == NULL)
  {
    fprintf(stderr, "Failed to initialize Lua");
    abort();
  }
  luaL_openlibs(lua_state);

  lj_init(lua_state, jvm, jvmti);
  /* this must be called AFTER lj_init() so the JVMTI callbacks can be registered */
  if (luaL_dostring(lua_state, "require('debuglib')"))
  {
    fprintf(stderr, "Failed to load debuglib.lua: %s\n", lua_tostring(lua_state, -1));
    abort();
  }
  new_jmonitor(lua_state, thread_resume_monitor, "yellow_tree_thread_resume_monitor");
  lua_setglobal(lua_state, "thread_resume_monitor");
}

void lua_start_cmd(const char *opts)
{
  lua_State *L = lua_newthread(lua_state);
  lua_getglobal(L, "setopts"); /* from debuglib.lua */
  lua_pushstring(L, opts);
  if (lua_pcall(L, 1, 0, 0))
  {
    fprintf(stderr, "Error setting options: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  luaL_dostring(L, "start_cmd()");
}

void lua_start_evp()
{
  lua_State *L = lua_newthread(lua_state);
  luaL_dostring(L, "start_evp()");
}
