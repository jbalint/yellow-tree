/* subordinate code to lua_java.c for handling JVMTI event callbacks */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <classfile_constants.h>

#include "myjni.h"
#include "jni_util.h"
#include "lua_java.h"
#include "lua_interface.h"
#include "java_bridge.h"
#include "lj_internal.h"

/* from lua_java.c */
extern lua_State *lj_L;

void lj_check_jvmti_error(lua_State *L);

extern jthread lj_current_thread;

/* function references for callback functions */
static struct {
  int cb_breakpoint_ref;
  int cb_method_entry_ref;
  int cb_method_exit_ref;
  int cb_single_step_ref;
} lj_jvmti_callbacks;

void lj_init_jvmti_event()
{
  /* clear callback refs */
  lj_jvmti_callbacks.cb_breakpoint_ref = LUA_NOREF;
  lj_jvmti_callbacks.cb_method_entry_ref = LUA_NOREF;
  lj_jvmti_callbacks.cb_method_exit_ref = LUA_NOREF;
  lj_jvmti_callbacks.cb_single_step_ref = LUA_NOREF;
}

static void disable_events_before_callback_handling(lua_State *L)
{
  /* disable events for the duration of the callback to prevent re-entering */
  lj_err = EV_DISABLET(SINGLE_STEP, get_current_java_thread());
  lj_check_jvmti_error(L);
  lj_err = EV_DISABLET(METHOD_EXIT, get_current_java_thread());
  lj_check_jvmti_error(L);
}

static void enable_events_after_callback_handling(lua_State *L)
{
  /* re-enable events unless they are disabled (maybe have been changed during callback) */
  if (lj_jvmti_callbacks.cb_single_step_ref != LUA_NOREF)
  {
	lj_err = EV_ENABLET(SINGLE_STEP, get_current_java_thread());
	lj_check_jvmti_error(lj_L);
  }
  else if (lj_jvmti_callbacks.cb_method_exit_ref != LUA_NOREF)
  {
	lj_err = EV_ENABLET(METHOD_EXIT, get_current_java_thread());
	lj_check_jvmti_error(lj_L);
  }
}

static void JNICALL cb_breakpoint(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread,
								  jmethodID method_id, jlocation location)
{
  /* all following callbacks are a copy of this code, changed for the
     callback ref and the callback arguments */
  int ref = lj_jvmti_callbacks.cb_breakpoint_ref;
  lua_State *L;

  if (ref == LUA_NOREF)
    return;

  L = lua_newthread(lj_L);

  lj_current_thread = (*jni)->NewGlobalRef(jni, thread);
  assert(lj_current_thread);

  disable_events_before_callback_handling(lj_L);

  lua_pushcfunction(L, lua_print_traceback);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_pushinteger(L, location);
  lua_pcall(L, 3, 0, -5);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */

  enable_events_after_callback_handling(lj_L);
}

static void JNICALL cb_method_entry(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method_id)
{
  int ref = lj_jvmti_callbacks.cb_method_entry_ref;
  lua_State *L;

  if (ref == LUA_NOREF)
    return;

  L = lua_newthread(lj_L);

  lj_current_thread = (*jni)->NewGlobalRef(jni, thread);
  assert(lj_current_thread);

  disable_events_before_callback_handling(lj_L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_call(L, 2, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */

  enable_events_after_callback_handling(lj_L);
}

static void JNICALL cb_method_exit(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method_id,
								   jboolean was_popped_by_exception, jvalue return_value)
{
  int ref = lj_jvmti_callbacks.cb_method_exit_ref;
  lua_State *L;

  if (ref == LUA_NOREF)
    return;

  L = lua_newthread(lj_L);

  lj_current_thread = (*jni)->NewGlobalRef(jni, thread);
  assert(lj_current_thread);

  disable_events_before_callback_handling(lj_L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_pushboolean(L, was_popped_by_exception);
  /* TODO return_value must be passed to Lua */
  lua_call(L, 3, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */

  enable_events_after_callback_handling(lj_L);
}

static void JNICALL cb_single_step(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID method_id,
									 jlocation location)
{
  int ref = lj_jvmti_callbacks.cb_single_step_ref;
  lua_State *L;

  if (ref == LUA_NOREF)
    return;

  L = lua_newthread(lj_L);

  lj_current_thread = (*jni)->NewGlobalRef(jni, thread);
  assert(lj_current_thread);

  disable_events_before_callback_handling(lj_L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_pushinteger(L, location);
  lua_call(L, 3, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */

  enable_events_after_callback_handling(lj_L);
}

static void get_jvmti_callback_pointers(const char *callback,
										void ***jvmti_function_ptr_ptr,
										void **lj_function_ptr, 
										int **ref_ptr)
{
  int *x;
  void *y;
  void **z;
  jvmtiEventCallbacks *evCbs = get_jvmti_callbacks();
  if (!ref_ptr)
    ref_ptr = &x;
  if (!lj_function_ptr)
    lj_function_ptr = &y;
  if (!jvmti_function_ptr_ptr)
    jvmti_function_ptr_ptr = &z;

  *ref_ptr = NULL;
  *jvmti_function_ptr_ptr = NULL;
  *lj_function_ptr = NULL;

  /* TODO there might(...?) be a better way to map all these callbacks */
  if (!strcmp(callback, "breakpoint"))
  {
    *jvmti_function_ptr_ptr = (void **)&evCbs->Breakpoint;
    *lj_function_ptr = cb_breakpoint;
    *ref_ptr = &lj_jvmti_callbacks.cb_breakpoint_ref;
  }
  else if (!strcmp(callback, "method_entry"))
  {
    *jvmti_function_ptr_ptr = (void **)&evCbs->MethodEntry;
    *lj_function_ptr = cb_method_entry;
    *ref_ptr = &lj_jvmti_callbacks.cb_method_entry_ref;
  }
  else if (!strcmp(callback, "method_exit"))
  {
    *jvmti_function_ptr_ptr = (void **)&evCbs->MethodExit;
    *lj_function_ptr = cb_method_exit;
    *ref_ptr = &lj_jvmti_callbacks.cb_method_exit_ref;
  }
  else if (!strcmp(callback, "single_step"))
  {
    *jvmti_function_ptr_ptr = (void **)&evCbs->SingleStep;
    *lj_function_ptr = cb_single_step;
    *ref_ptr = &lj_jvmti_callbacks.cb_single_step_ref;
  }
}

int lj_set_jvmti_callback(lua_State *L)
{
  const char *callback;
  int ref;
  jvmtiEventCallbacks *evCbs;
  void **jvmti_callback_ptr;
  void *lj_callback_ptr;
  int *ref_ptr;

  callback = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  ref = luaL_ref(L, LUA_REGISTRYINDEX); /* this pops.. */
  lua_pop(L, 1);

  evCbs = get_jvmti_callbacks();

  if (!ref)
  {
    (void)luaL_error(L, "Unknown callback '%s'\n", callback);
  }

  /* special handling for specific callback types */
  lj_err = JVMTI_ERROR_NONE;
  if (!strcmp(callback, "single_step"))
    lj_err = EV_ENABLET(SINGLE_STEP, get_current_java_thread());
  else if (!strcmp(callback, "method_exit"))
	lj_err = EV_ENABLET(METHOD_EXIT, get_current_java_thread());
  lj_check_jvmti_error(L);

  get_jvmti_callback_pointers(callback, &jvmti_callback_ptr, &lj_callback_ptr, &ref_ptr);
  *jvmti_callback_ptr = lj_callback_ptr;
  *ref_ptr = ref;

  lj_err = (*current_jvmti())->SetEventCallbacks(current_jvmti(), evCbs, sizeof(jvmtiEventCallbacks));
  lj_check_jvmti_error(L);

  return 0;
}

int lj_clear_jvmti_callback(lua_State *L)
{
  const char *callback;
  void **jvmti_callback_ptr;
  int *ref_ptr;
  jvmtiEventCallbacks *evCbs;

  callback = luaL_checkstring(L, 1);
  lua_pop(L, 1);

  evCbs = get_jvmti_callbacks();

  lj_err = JVMTI_ERROR_NONE;
  if (!strcmp(callback, "single_step"))
    EV_DISABLET(SINGLE_STEP, get_current_java_thread());
  else if (!strcmp(callback, "method_exit"))
	EV_DISABLET(METHOD_EXIT, get_current_java_thread());
  lj_check_jvmti_error(L);

  get_jvmti_callback_pointers(callback, &jvmti_callback_ptr, NULL, &ref_ptr);
  *jvmti_callback_ptr = NULL;
  *ref_ptr = LUA_NOREF;

  lj_err = (*current_jvmti())->SetEventCallbacks(current_jvmti(), evCbs, sizeof(jvmtiEventCallbacks));
  lj_check_jvmti_error(L);

  return 0;
}
