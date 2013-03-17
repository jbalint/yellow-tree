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
    lj_print_message("Failed to initialize Lua");
    abort();
  }
  luaL_openlibs(lua_state);
  if (luaL_dostring(lua_state, "require('java_bridge')"))
  {
    lj_print_message("Failed to load java_bridge.lua: %s\n", lua_tostring(lua_state, -1));
    abort();
  }
  /* this must be called AFTER java_bridge.lua is loaded so the metatables for the Java types
     are created */
  lj_init(lua_state, jvmti);
  /* this must be called AFTER lj_init() so the JVMTI callbacks can be registered */
  if (luaL_dostring(lua_state, "require('debuglib')"))
  {
    lj_print_message("Failed to load debuglib.lua: %s\n", lua_tostring(lua_state, -1));
    abort();
  }
  lj_set_jvm_exec_monitor(mon);
}

void lua_command_loop(JNIEnv *jni)
{
  lj_set_jni(jni);
  luaL_dostring(lua_state, "command_loop()");
}
