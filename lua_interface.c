#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "lua_interface.h"
#include "lua_java.h"

static lua_State *lua_state;

void lua_interface_init() {
  lua_state = luaL_newstate();
  if (lua_state == NULL) {
    fprintf(stderr, "Failed to initialize Lua");
    abort();
  }
  luaL_openlibs(lua_state);
  lj_init(lua_state);
}

void lua_command_loop() {
  char cmd[255];
  size_t len;

  printf("Starting lua command loop\n");

  printf("yt> ");
  while(fgets(cmd, 255, stdin))
  {
    len = strlen(cmd) - 1;
    cmd[len] = 0;
    if(!strcmp(cmd, "run") || !strcmp(cmd, "cont"))
      break;
    else if(!strcmp(cmd, "where"))
      /*dump_stack(-1, 0);*/
      printf("dump stack\n");
    else
    {
      if (luaL_dostring(lua_state, cmd)) {
	printf("%s\n", lua_tostring(lua_state, -1));
      }
    }
    printf("yt> ");
  }
}

