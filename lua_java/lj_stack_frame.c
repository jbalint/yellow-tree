#include "lua_java.h"
#include "java_bridge.h"
#include "lj_internal.h"

static int lj_get_frame_count(lua_State *L)
{
  jint count;
  jobject thread = *(jobject *)luaL_checkudata(L, 1, "jobject");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetFrameCount(current_jvmti(), thread, &count);
  lj_check_jvmti_error(L);
  lua_pushinteger(L, count);
  return 1;
}

static int lj_get_stack_frame(lua_State *L)
{
  jvmtiFrameInfo fi;
  jint count;
  jobject thread = *(jobject *)luaL_checkudata(L, 1, "jobject");
  int frame_num = luaL_checkint(L, 2);
  lua_pop(L, 2);

  /* get stack frame info */
  lj_err = (*current_jvmti())->GetStackTrace(current_jvmti(), thread, frame_num - 1, 1, &fi, &count);
  lj_check_jvmti_error(L);
  if (count == 0) {
    return 0;
  }
  assert(count == 1);

  lua_newtable(L);

  lua_pushinteger(L, fi.location);
  lua_setfield(L, -2, "location");
  new_jmethod_id(L, fi.method);
  lua_setfield(L, -2, "method_id");
  lua_pushinteger(L, frame_num);
  lua_setfield(L, -2, "depth");

  return 1;
}

void lj_stack_frame_register(lua_State *L)
{
  lua_register(L, "lj_get_frame_count",            lj_get_frame_count);
  lua_register(L, "lj_get_stack_frame",            lj_get_stack_frame);
}
