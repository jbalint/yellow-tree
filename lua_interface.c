#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "lua_interface.h"
#include "lua_java.h"
#include "java_bridge.h"

static lua_State *lua_state;

/* lazy, from 
http://stackoverflow.com/questions/12256455/print-stacktrace-from-c-code-with-embedded-lua
*/
int traceback (lua_State *L) {
  if (!lua_isstring(L, 1))  /* 'message' not a string? */
    return 1;  /* keep it intact */
  lua_getglobal(L, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

int lua_print_traceback (lua_State *L)
{
  const char *stack;
  traceback(L);
  stack = lua_tostring(L, -1);
  fprintf(stderr, "<lua_print_traceback> %s\n", stack);
  lua_pop(L, 2);
  return 0;
}

int lua_interface_error(lua_State *L, const char *format, ...)
{
  va_list ap;
  char msg[500];
  va_start(ap, format);
  vsprintf(msg, format, ap);
  fflush(stdout);
  va_end(ap);
  lua_pushstring(L, msg);
  traceback(L);
  return lua_error(L);
}

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

  lua_pushcfunction(lua_state, traceback);
  /* this must be called AFTER lj_init() so the JVMTI callbacks can be registered */
  if (luaL_loadstring(lua_state, "require('debuglib')") || lua_pcall(lua_state, 0, 0, -2))
  {
    fprintf(stderr, "Failed to load debuglib.lua: %s\n", lua_tostring(lua_state, -1));
    abort();
  }
  lua_remove(lua_state, -1);

  lua_pushcfunction(lua_state, traceback);
  lua_getglobal(lua_state, "jmonitor");
  lua_getfield(lua_state, -1, "create");
  lua_remove(lua_state, -2);
  new_jmonitor(lua_state, thread_resume_monitor, "yellow_tree_thread_resume_monitor");
  if (lua_pcall(lua_state, 1, 1, -3))
  {
    fprintf(stderr, "Failed to initialize: %s\n", lua_tostring(lua_state, -1));
    abort();
  }
  lua_remove(lua_state, -2);
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

  lua_pushcfunction(L, traceback);
  while (1)
  {
    lua_getglobal(L, "start_cmd");
    if (lua_pcall(L, 0, 0, -2))
    {
      fprintf(stderr, "Error during command interpreter: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
    /* allow exiting intentionally here, if start_cmd() returns true */
    if (lua_gettop(L) == 1)
    {
      int res = lua_toboolean(L, 1);
	  if (res)
		break;
    }
  }
}
