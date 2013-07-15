#include "myjni.h"
#include "jni_util.h"
#include "lua_interface.h"
#include "lua_java.h"
#include "java_bridge.h"
#include "lj_internal.h"

/* Lua wrappers for raw monitor operations */

static int lj_create_raw_monitor(lua_State *L)
{
  jrawMonitorID monitor;
  const char *name;

  name = luaL_checkstring(L, 1);
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->CreateRawMonitor(current_jvmti(), name, &monitor);
  lj_check_jvmti_error(L);

  new_jmonitor(L, monitor, name);

  return 1;
}

static int lj_destroy_raw_monitor(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->DestroyRawMonitor(current_jvmti(), monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_enter(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->RawMonitorEnter(current_jvmti(), monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_exit(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->RawMonitorExit(current_jvmti(), monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_wait(lua_State *L)
{
  jrawMonitorID monitor;
  jlong wait;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor");
  wait = luaL_checkint(L, 2);
  lua_pop(L, 2);

  lj_err = (*current_jvmti())->RawMonitorWait(current_jvmti(), monitor, wait);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_notify(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->RawMonitorNotify(current_jvmti(), monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_notify_all(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->RawMonitorNotifyAll(current_jvmti(), monitor);
  lj_check_jvmti_error(L);

  return 0;
}

void lj_raw_monitor_register(lua_State *L)
{
  lua_register(L, "lj_create_raw_monitor",         lj_create_raw_monitor);
  lua_register(L, "lj_destroy_raw_monitor",        lj_destroy_raw_monitor);
  lua_register(L, "lj_raw_monitor_enter",          lj_raw_monitor_enter);
  lua_register(L, "lj_raw_monitor_exit",           lj_raw_monitor_exit);
  lua_register(L, "lj_raw_monitor_wait",           lj_raw_monitor_wait);
  lua_register(L, "lj_raw_monitor_notify",         lj_raw_monitor_notify);
  lua_register(L, "lj_raw_monitor_notify_all",     lj_raw_monitor_notify_all);
}
