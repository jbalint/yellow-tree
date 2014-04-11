#include "lua_java.h"
#include "lj_internal.h"

static int lj_force_early_return_object(lua_State *L)
{
  jobject thread = *(jobject *)luaL_checkudata(L, 1, "jobject");
  jobject ret_val = NULL;
  
  if (lua_type(L, 2) != LUA_TNIL)
	ret_val = *(jobject *)luaL_checkudata(L, 2, "jobject");

  lj_err = (*current_jvmti())->ForceEarlyReturnObject(current_jvmti(), thread, ret_val);
  lj_check_jvmti_error(L);

  lua_pop(L, 2);

  return 0;
}

static int lj_force_early_return_int(lua_State *L)
{
  jobject thread = *(jobject *)luaL_checkudata(L, 1, "jobject");
  int retval;
  if (lua_isboolean(L, 2))
	retval = lua_toboolean(L, 2);
  else
	retval = luaL_checkint(L, 2);
  lj_err = (*current_jvmti())->ForceEarlyReturnInt(current_jvmti(), thread, retval);
  lj_check_jvmti_error(L);
  lua_pop(L, 2);
  return 0;
}

static int lj_force_early_return_long(lua_State *L)
{
  assert(0);// TODO
  return 0;
}

static int lj_force_early_return_float(lua_State *L)
{
  assert(0);// TODO
  return 0;
}

static int lj_force_early_return_double(lua_State *L)
{
  assert(0);// TODO
  return 0;
}

static int lj_force_early_return_void(lua_State *L)
{
  jobject thread = *(jobject *)luaL_checkudata(L, 1, "jobject");
  lua_pop(L, 1);
  lj_err = (*current_jvmti())->ForceEarlyReturnVoid(current_jvmti(), thread);
  lj_check_jvmti_error(L);
  return 0;
}

void lj_force_early_return_register(lua_State *L)
{
  lua_register(L, "lj_force_early_return_object",  lj_force_early_return_object);
  lua_register(L, "lj_force_early_return_int",     lj_force_early_return_int);
  lua_register(L, "lj_force_early_return_long",    lj_force_early_return_long);
  lua_register(L, "lj_force_early_return_float",   lj_force_early_return_float);
  lua_register(L, "lj_force_early_return_double",  lj_force_early_return_double);
  lua_register(L, "lj_force_early_return_void",    lj_force_early_return_void);
}
