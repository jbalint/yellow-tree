#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <classfile_constants.h>

#include "myjni.h"
#include "jni_util.h"
#include "lua_java.h"

 /*  _    _ _   _ _      */
 /* | |  | | | (_) |     */
 /* | |  | | |_ _| |___  */
 /* | |  | | __| | / __| */
 /* | |__| | |_| | \__ \ */
 /*  \____/ \__|_|_|___/ */

static jvmtiEnv *lj_jvmti;
static jvmtiError lj_err;

static JavaVM *lj_jvm;

/* TODO: is this used for anything */
static jthread lj_current_thread;

/* needed to have a lua state at jvmti callback */
static lua_State *lj_L;
/* we need to keep class paired with field_id due to JVMTI API */
typedef struct {
  jfieldID field_id;
  jclass class;
} lj_field_id;
/* function references for callback functions */
static struct {
  int cb_breakpoint_ref;
  int cb_method_entry_ref;
  int cb_method_exit_ref;
  int cb_single_step_ref;
} lj_jvmti_callbacks;

/* marker where NULL is used as a jthread param for current thread */
#define NULL_JTHREAD NULL

#define EV_ENABLET(EVTYPE, EVTHR) \
  (lj_err = event_change(lj_jvmti, JVMTI_ENABLE,	\
						 JVMTI_EVENT_##EVTYPE, (EVTHR)))
#define EV_DISABLET(EVTYPE, EVTHR) \
  (lj_err = event_change(lj_jvmti, JVMTI_DISABLE,	\
						 JVMTI_EVENT_##EVTYPE, (EVTHR)))

static JNIEnv *current_jni()
{
  JNIEnv *jni;
  jint ret = (*lj_jvm)->AttachCurrentThread(lj_jvm, (void**)&jni, NULL);
  assert(ret == JNI_OK);
  return jni;
}

static void lj_check_jvmti_error(lua_State *L)
{
  char *errmsg = "<Unknown Error>";
  if(lj_err == JVMTI_ERROR_NONE)
    return;

  (*lj_jvmti)->GetErrorName(lj_jvmti, lj_err, &errmsg);
  /* we never Deallocate() the errmsg returned from JVMTI */

  if (IsDebuggerPresent())
    DebugBreak();

  (void)luaL_error(L, "Error %d from JVMTI: %s", lj_err, errmsg);
}

/* allocate a new userdata object for a jmethodID */
void new_jmethod_id(lua_State *L, jmethodID method_id)
{
  jmethodID *user_data;
  user_data = lua_newuserdata(L, sizeof(jmethodID));
  *user_data = method_id;
  lua_getfield(L, LUA_REGISTRYINDEX, "jmethod_id_mt");
  lua_setmetatable(L, -2);
}

void new_jfield_id(lua_State *L, jfieldID field_id, jclass class)
{
  lj_field_id *user_data;
  user_data = lua_newuserdata(L, sizeof(lj_field_id));
  user_data->field_id = field_id;
  user_data->class = class;
  lua_getfield(L, LUA_REGISTRYINDEX, "jfield_id_mt");
  lua_setmetatable(L, -2);
}

void new_jobject(lua_State *L, jobject object)
{
  jobject *user_data;
  if (!object)
  {
    lua_pushnil(L);
    return;
  }
  user_data = lua_newuserdata(L, sizeof(jobject));
  *user_data = object;
  lua_getfield(L, LUA_REGISTRYINDEX, "jobject_mt");
  lua_setmetatable(L, -2);
}

void new_jmonitor(lua_State *L, jrawMonitorID monitor, const char *name)
{
  jrawMonitorID *user_data;
  user_data = lua_newuserdata(L, sizeof(jrawMonitorID));
  *user_data = monitor;
  lua_getfield(L, LUA_REGISTRYINDEX, "jmonitor_mt");
  lua_setmetatable(L, -2);
  assert(name);
  (void)name;
}

void new_string(lua_State *L, JNIEnv *jni, jstring string)
{
  const char *utf_chars;
  utf_chars = (*jni)->GetStringUTFChars(jni, string, NULL);
  if (utf_chars)
  {
    lua_pushstring(L, utf_chars);
    (*jni)->ReleaseStringUTFChars(jni, string, utf_chars);
    EXCEPTION_CHECK(jni);
  }
  else
  {
    lua_pushnil(L);
  }
}

static jobject get_current_java_thread()
{
  JNIEnv *jni = current_jni();
  jclass thread_class;
  jmethodID getCurrentThread_method_id;
  jobject current_thread;

  if (lj_current_thread)
    return lj_current_thread;

  thread_class = (*jni)->FindClass(jni, "java/lang/Thread");
  EXCEPTION_CHECK(jni);
  assert(thread_class);

  getCurrentThread_method_id = (*jni)->GetStaticMethodID(jni, thread_class,
														 "currentThread",
														 "()Ljava/lang/Thread;");
  EXCEPTION_CHECK(jni);
  assert(getCurrentThread_method_id);

  current_thread = (*jni)->CallStaticObjectMethod(jni, thread_class,
												  getCurrentThread_method_id);
  EXCEPTION_CHECK(jni);
  assert(current_thread);

  return current_thread;
}

 /*  _                   ______                _   _                  */
 /* | |                 |  ____|              | | (_)                 */
 /* | |    _   _  __ _  | |__ _   _ _ __   ___| |_ _  ___  _ __  ___  */
 /* | |   | | | |/ _` | |  __| | | | '_ \ / __| __| |/ _ \| '_ \/ __| */
 /* | |___| |_| | (_| | | |  | |_| | | | | (__| |_| | (_) | | | \__ \ */
 /* |______\__,_|\__,_| |_|   \__,_|_| |_|\___|\__|_|\___/|_| |_|___/ */

static int lj_get_frame_count(lua_State *L)
{
  jint count;
  jobject thread;

  /* check if we're passed a thread */
  if (lua_gettop(L) == 1)
  {
    thread = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
    lua_pop(L, 1);
  }
  else
  {
    thread = get_current_java_thread();
  }

  lj_err = (*lj_jvmti)->GetFrameCount(lj_jvmti, thread, &count);
  lj_check_jvmti_error(L);
  lua_pushinteger(L, count);
  return 1;
}

static int lj_get_stack_frame(lua_State *L)
{
  int frame_num;
  jvmtiFrameInfo fi;
  jint count;
  jobject thread;
  int args = 1;

  frame_num = luaL_checkint(L, 1);
  /* check if we're passed a thread */
  if (lua_gettop(L) == 2)
  {
    args++;
    thread = *(jobject *)luaL_checkudata(L, 2, "jobject_mt");
  }
  else
  {
    thread = get_current_java_thread();
  }
  lua_pop(L, args);

  /* get stack frame info */
  lj_err = (*lj_jvmti)->GetStackTrace(lj_jvmti, thread, frame_num-1, 1, &fi, &count);
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

static int lj_set_breakpoint(lua_State *L)
{
  jmethodID method_id;
  jlocation location;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  location = luaL_checkinteger(L, 2);
  lua_pop(L, 2);

  lj_err = (*lj_jvmti)->SetBreakpoint(lj_jvmti, method_id, location);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_clear_breakpoint(lua_State *L)
{
  jmethodID method_id;
  jlocation location;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  location = luaL_checkinteger(L, 2);
  lua_pop(L, 2);

  lj_err = (*lj_jvmti)->ClearBreakpoint(lj_jvmti, method_id, location);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_get_local_variable_table(lua_State *L)
{
  jmethodID method_id;
  jvmtiLocalVariableEntry *vars;
  jint count;
  int i;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetLocalVariableTable(lj_jvmti, method_id, &count, &vars);
  if (lj_err == JVMTI_ERROR_ABSENT_INFORMATION)
  {
    lua_pushnil(L);
    return 1;
  }
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < count; ++i)
  {
    /* create the var entry */
    lua_newtable(L);

    lua_pushstring(L, vars[i].name);
    lua_setfield(L, -2, "name");

    lua_pushstring(L, vars[i].signature);
    lua_setfield(L, -2, "sig");

    lua_pushinteger(L, vars[i].start_location);
    lua_setfield(L, -2, "start_location");

    lua_pushinteger(L, vars[i].slot);
    lua_setfield(L, -2, "slot");

    /* add it to the return table */
    lua_setfield(L, -2, vars[i].name);
  }

  free_jvmti_refs(lj_jvmti, vars, (void *)-1);

  return 1;
}

static int lj_get_method_id(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jclass class;
  jmethodID method_id;
  const char *class_name;
  const char *method_name;
  const char *args;
  const char *ret;
  char *sig;

  class_name = luaL_checkstring(L, 1);
  method_name = luaL_checkstring(L, 2);
  args = luaL_checkstring(L, 3);
  ret = luaL_checkstring(L, 4);
  lua_pop(L, 4);

  /* get class */
  class = (*jni)->FindClass(jni, class_name);
  EXCEPTION_CLEAR(jni);
  if (class == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  /* build signature string */
  sig = malloc(strlen(args) + strlen(ret) + 10);
  sprintf(sig, "(%s)%s", args, ret);

  /* try instance method */
  method_id = (*jni)->GetMethodID(jni, class, method_name, sig);
  EXCEPTION_CLEAR(jni);

  /* otherwise try static method */
  if (method_id == NULL)
    method_id = (*jni)->GetStaticMethodID(jni, class, method_name, sig);
  EXCEPTION_CLEAR(jni);
  free(sig);

  if (method_id == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  new_jmethod_id(L, method_id);

  return 1;
}

/**
 * Convenience function to see if we can return a nil
 * instead of throwing an error.
 */
static int local_variable_is_nil(jvmtiError err)
{
  if (err == JVMTI_ERROR_INVALID_SLOT)
    return 1;
  else if (err == JVMTI_ERROR_TYPE_MISMATCH)
    return 1;
  else if (err == JVMTI_ERROR_NULL_POINTER)
    return 1;
  else
    return 0;
}

static int lj_get_local_variable(lua_State *L)
{
  jint depth;
  jint slot;
  const char *type;

  jint val_i;
  jlong val_j;
  jfloat val_f;
  jdouble val_d;
  jobject val_l;

  depth = luaL_checkinteger(L, 1);
  slot = luaL_checkinteger(L, 2);
  type = luaL_checkstring(L, 3);
  lua_pop(L, 3);
  if (!strcmp(type, "Z") ||
      !strcmp(type, "B") ||
      !strcmp(type, "C") ||
      !strcmp(type, "S") ||
      !strcmp(type, "I"))
  {
    lj_err = (*lj_jvmti)->GetLocalInt(lj_jvmti, get_current_java_thread(),
									  depth-1, slot, &val_i);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      if (!strcmp(type, "Z"))
	lua_pushboolean(L, val_i);
      else
	lua_pushinteger(L, val_i);
    }
  }
  else if (!strcmp(type, "J"))
  {
    lj_err = (*lj_jvmti)->GetLocalLong(lj_jvmti, get_current_java_thread(),
									   depth-1, slot, &val_j);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushinteger(L, val_j);
    }
  }
  else if (!strcmp(type, "F"))
  {
    lj_err = (*lj_jvmti)->GetLocalFloat(lj_jvmti, get_current_java_thread(),
										depth-1, slot, &val_f);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushnumber(L, val_f);
    }
  }
  else if (!strcmp(type, "D"))
  {
    lj_err = (*lj_jvmti)->GetLocalDouble(lj_jvmti, get_current_java_thread(),
										 depth-1, slot, &val_d);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushnumber(L, val_d);
    }
  }
  else if (*type == 'L' || *type == '[')
  {
    /* GetLocalInstance() is new to JVMTI 1.2 */
    /* if (slot == 0) */
    /*   lj_err = (*lj_jvmti)->GetLocalInstance(lj_jvmti, get_current_java_thread(), depth-1, &val_l); */
    /* else */
    lj_err = (*lj_jvmti)->GetLocalObject(lj_jvmti, get_current_java_thread(),
										 depth-1, slot, &val_l);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      new_jobject(L, val_l);
    }
  }
  else
  {
    lua_pushnil(L);
  }

  return 1;
}

static int lj_pointer_to_string(lua_State *L)
{
  char buf[20];
  const void *p;
  p = lua_topointer(L, -1);
  lua_pop(L, 1);
  sprintf(buf, "%p", *(jobject *)p);
  lua_pushstring(L, buf);
  return 1;
}

static int lj_get_class_methods(lua_State *L)
{
  jint method_count;
  jmethodID *methods;
  jclass class;
  int i;

  class = *(jclass *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetClassMethods(lj_jvmti, class, &method_count, &methods);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < method_count; ++i)
  {
    new_jmethod_id(L, methods[i]);
    lua_rawseti(L, -2, i+1);
  }

  free_jvmti_refs(lj_jvmti, methods, (void *)-1);

  return 1;
}

static int lj_find_class(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jclass class;
  const char *class_name;

  class_name = luaL_checkstring(L, 1);
  lua_pop(L, 1);

  class = (*jni)->FindClass(jni, class_name);
  if (class == NULL)
  {
	/* most of the time, we'll be getting NoClassDefFoundError.
	   It's not worth the effort to detect and report other exceptions here... */
	(*jni)->ExceptionClear(jni);
	lua_pushnil(L);
  }
  else
  {
	new_jobject(L, class);
  }

  return 1;
}

/**
 * Calling a Java method requires the following parameters:
 * - object - target of method call
 * - method id
 * - return type - I, STR, Ljava/lang/Object;, etc
 * - arg count - number of arguments
 * - arguments(varargs...) - pairs of type/arg
 */
static int lj_call_method(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jobject object;
  jmethodID method_id;
  const char *ret;
  jvalue val;
  int argcount;
  int i;
  int is_static;
  int param_num;
  int result_count = 1;

  jvalue *jargs = NULL;

  const char *argtype;
  char *method_name;

  object = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  method_id = *(jmethodID *)luaL_checkudata(L, 2, "jmethod_id_mt");
  is_static = lua_toboolean(L, 3);
  ret = luaL_checkstring(L, 4);
  argcount = luaL_checkinteger(L, 5);

  (*lj_jvmti)->GetMethodName(lj_jvmti, method_id, &method_name, NULL, NULL);
  lj_check_jvmti_error(L);

  if (argcount > 0)
  {
    jargs = malloc(sizeof(jvalue) * argcount);
    memset(jargs, 0, sizeof(jvalue) * argcount);
  }

  param_num = 6;
  /* get arguments */
  for (i = 0; i < argcount; ++i)
  {
    argtype = luaL_checkstring(L, param_num++);
    assert(argtype);
    if (!strcmp("V", argtype))
    {
      jargs[i].l = NULL;
      param_num++; /* skip the nil */
    }
    else if ('L' == argtype[0] || '[' == argtype[0])
    {
      jargs[i].l = *(jobject *)luaL_checkudata(L, param_num++, "jobject_mt");
    }
    else if (!strcmp("STR", argtype)) /* TODO non-standard indicator */
    {
      jargs[i].l = (*jni)->NewStringUTF(jni, luaL_checkstring(L, param_num++));
    }
    else if (!strcmp("Z", argtype))
    {
      luaL_checktype(L, param_num, LUA_TBOOLEAN);
      jargs[i].z = lua_toboolean(L, param_num);
      param_num++;
    }
    else if (!strcmp("B", argtype))
    {
      jargs[i].b = luaL_checkinteger(L, param_num++);
    }
    else if (!strcmp("C", argtype)) /* TODO should this be a one-element string? (not int) */
    {
      jargs[i].c = luaL_checkinteger(L, param_num++);
    }
    else if (!strcmp("S", argtype))
    {
      jargs[i].s = luaL_checkinteger(L, param_num++);
    }
    else if (!strcmp("I", argtype))
    {
      jargs[i].i = luaL_checkinteger(L, param_num++);
    }
    else if (!strcmp("J", argtype))
    {
      jargs[i].j = luaL_checkinteger(L, param_num++);
    }
    else if (!strcmp("F", argtype))
    {
      jargs[i].f = luaL_checknumber(L, param_num++);
    }
    else if (!strcmp("D", argtype))
    {
      jargs[i].d = luaL_checknumber(L, param_num++);
    }
    else
    {
      (void)luaL_error(L, "Unknown argument type '%s' for argument %d\n", argtype, i);
    }
  }

  lua_pop(L, (2 * argcount) + 5);

  memset(&val, 0, sizeof(val));
  /* call method - in order shown in JNI docs*/
  if (!strcmp("V", ret))
  {
    if (is_static)
      (*jni)->CallStaticVoidMethodA(jni, object, method_id, jargs);
    else
      (*jni)->CallVoidMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    result_count = 0;
  }
  else if (ret[0] == 'L' || ret[0] == '[' || !strcmp("STR", ret))
  {
    if (!strcmp("<init>", method_name))
      val.l = (*jni)->NewObjectA(jni, object, method_id, jargs);
    else if (is_static)
      val.l = (*jni)->CallStaticObjectMethodA(jni, object, method_id, jargs);
    else
      val.l = (*jni)->CallObjectMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    if (!strcmp("L", ret) || ret[0] == '[')
      new_jobject(L, val.l);
    else
      new_string(L, jni, val.l);
  }
  else if (!strcmp("Z", ret))
  {
    if (is_static)
      val.z = (*jni)->CallStaticBooleanMethodA(jni, object, method_id, jargs);
    else
      val.z = (*jni)->CallBooleanMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushboolean(L, val.z);
  }
  else if (!strcmp("B", ret))
  {
    if (is_static)
      val.b = (*jni)->CallStaticByteMethodA(jni, object, method_id, jargs);
    else
      val.b = (*jni)->CallByteMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.b);
  }
  else if (!strcmp("C", ret))
  {
    if (is_static)
      val.c = (*jni)->CallStaticCharMethodA(jni, object, method_id, jargs);
    else
      val.c = (*jni)->CallCharMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.c);
  }
  else if (!strcmp("S", ret))
  {
    if (is_static)
      val.s = (*jni)->CallStaticShortMethodA(jni, object, method_id, jargs);
    else
      val.s = (*jni)->CallShortMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.s);
  }
  else if (!strcmp("I", ret))
  {
    if (is_static)
      val.i = (*jni)->CallStaticIntMethodA(jni, object, method_id, jargs);
    else
      val.i = (*jni)->CallIntMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.i);
  }
  else if (!strcmp("J", ret))
  {
    if (is_static)
      val.j = (*jni)->CallStaticLongMethodA(jni, object, method_id, jargs);
    else
      val.j = (*jni)->CallLongMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.j);
  }
  else if (!strcmp("F", ret))
  {
    if (is_static)
      val.f = (*jni)->CallStaticFloatMethodA(jni, object, method_id, jargs);
    else
      val.f = (*jni)->CallFloatMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushnumber(L, val.f);
  }
  else if (!strcmp("D", ret))
  {
    if (is_static)
      val.d = (*jni)->CallStaticDoubleMethodA(jni, object, method_id, jargs);
    else
      val.d = (*jni)->CallDoubleMethodA(jni, object, method_id, jargs);
    EXCEPTION_CHECK(jni);
    lua_pushnumber(L, val.d);
  }
  else
  {
    luaL_error(L, "Unknown return type '%s'", ret);
  }

  if (argcount > 0)
    free(jargs);
  free_jvmti_refs(lj_jvmti, method_name, (void *)-1);

  return result_count;
}

static int lj_toString(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jobject object;
  jclass class;
  jmethodID method_id;
  jstring string;

  object = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  class = (*jni)->FindClass(jni, "java/lang/Object");
  EXCEPTION_CHECK(jni);
  if (class == NULL)
  {
    lua_pushnil(L);
    return 1;
  }
  method_id = (*jni)->GetMethodID(jni, class, "toString", "()Ljava/lang/String;");
  EXCEPTION_CHECK(jni);
  if (method_id == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  string = (jstring)(*jni)->CallObjectMethod(jni, object, method_id);
  EXCEPTION_CHECK(jni);
  if (string == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  new_string(L, jni, string);

  return 1;
}

static int lj_get_method_name(lua_State *L)
{
  jmethodID method_id;
  char *method_name;
  char *sig;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetMethodName(lj_jvmti, method_id, &method_name, &sig, NULL);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  lua_pushstring(L, method_name);
  lua_setfield(L, -2, "name");
  lua_pushstring(L, sig);
  lua_setfield(L, -2, "sig");
  
  return 1;
}

static int lj_get_method_declaring_class(lua_State *L)
{
  jmethodID method_id;
  jclass class;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetMethodDeclaringClass(lj_jvmti, method_id, &class);
  lj_check_jvmti_error(L);

  new_jobject(L, class);

  return 1;
}

static int lj_get_method_modifiers(lua_State *L)
{
  jmethodID method_id;
  jint modifiers;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetMethodModifiers(lj_jvmti, method_id, &modifiers);
  lj_check_jvmti_error(L);

  lua_pushinteger(L, modifiers);

  return 1;
}

static int lj_get_method_modifiers_table(lua_State *L)
{
  lua_Integer modifiers;

  modifiers = luaL_checkinteger(L, 1);
  lua_pop(L, 1);

  lua_newtable(L);

  /* http://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.6 */
  lua_pushboolean(L, modifiers & JVM_ACC_PUBLIC);
  lua_setfield(L, -2, "public");
  lua_pushboolean(L, modifiers & JVM_ACC_PRIVATE);
  lua_setfield(L, -2, "private");
  lua_pushboolean(L, modifiers & JVM_ACC_PROTECTED);
  lua_setfield(L, -2, "protected");
  lua_pushboolean(L, modifiers & JVM_ACC_STATIC);
  lua_setfield(L, -2, "static");

  lua_pushboolean(L, modifiers & JVM_ACC_FINAL);
  lua_setfield(L, -2, "final");
  lua_pushboolean(L, modifiers & JVM_ACC_SYNCHRONIZED);
  lua_setfield(L, -2, "synchronized");
  lua_pushboolean(L, modifiers & JVM_ACC_BRIDGE);
  lua_setfield(L, -2, "bridge");
  lua_pushboolean(L, modifiers & JVM_ACC_VARARGS);
  lua_setfield(L, -2, "varargs");

  lua_pushboolean(L, modifiers & JVM_ACC_NATIVE);
  lua_setfield(L, -2, "native");
  lua_pushboolean(L, modifiers & JVM_ACC_ABSTRACT);
  lua_setfield(L, -2, "abstract");
  lua_pushboolean(L, modifiers & JVM_ACC_STRICT);
  lua_setfield(L, -2, "strict");
  lua_pushboolean(L, modifiers & JVM_ACC_SYNTHETIC);
  lua_setfield(L, -2, "synthetic");

  return 1;
}

static int lj_get_field_name(lua_State *L)
{
  lj_field_id *field_id;
  char *field_name;
  char *sig;

  field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetFieldName(lj_jvmti, field_id->class, field_id->field_id, &field_name, &sig, NULL);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  lua_pushstring(L, field_name);
  lua_setfield(L, -2, "name");
  lua_pushstring(L, sig);
  lua_setfield(L, -2, "sig");

  return 1;
}

static int lj_get_field_declaring_class(lua_State *L)
{
  lj_field_id *field_id;
  jclass class;

  field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetFieldDeclaringClass(lj_jvmti, field_id->class, field_id->field_id, &class);
  lj_check_jvmti_error(L);

  new_jobject(L, class);

  return 1;
}

static int lj_get_field_id(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jclass class;
  jfieldID field_id;
  const char *class_name;
  const char *field_name;
  const char *sig;

  class_name = luaL_checkstring(L, 1);
  field_name = luaL_checkstring(L, 2);
  sig = luaL_checkstring(L, 3);
  lua_pop(L, 3);

  /* get class */
  class = (*jni)->FindClass(jni, class_name);
  EXCEPTION_CLEAR(jni);
  if (class == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  /* try instance field */
  field_id = (*jni)->GetFieldID(jni, class, field_name, sig);
  EXCEPTION_CLEAR(jni);

  /* otherwise try static field */
  if (field_id == NULL)
    field_id = (*jni)->GetStaticFieldID(jni, class, field_name, sig);
  EXCEPTION_CLEAR(jni);

  if (field_id == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  new_jfield_id(L, field_id, class);

  return 1;
}

static int lj_get_class_fields(lua_State *L)
{
  jint field_count;
  jfieldID *fields;
  jclass class;
  int i;

  class = *(jclass *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetClassFields(lj_jvmti, class, &field_count, &fields);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < field_count; ++i)
  {
    new_jfield_id(L, fields[i], class);
    lua_rawseti(L, -2, i+1);
  }

  free_jvmti_refs(lj_jvmti, fields, (void *)-1);

  return 1;
}

static int lj_get_field(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jvalue val;
  jobject object;
  lj_field_id *field_id;
  char *sig;
  int is_static;

  object = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  field_id = (lj_field_id *)luaL_checkudata(L, 2, "jfield_id_mt");
  luaL_checktype(L, 3, LUA_TBOOLEAN);
  is_static = lua_toboolean(L, -1);
  lua_pop(L, 3);

  lj_err = (*lj_jvmti)->GetFieldName(lj_jvmti, field_id->class, field_id->field_id,
									 NULL, &sig, NULL);
  lj_check_jvmti_error(L);

  if (*sig == 'L' || *sig == '[')
  {
    if (is_static)
      val.l = (*jni)->GetStaticObjectField(jni, object, field_id->field_id);
    else
      val.l = (*jni)->GetObjectField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    new_jobject(L, val.l);
  }
  else if (!strcmp(sig, "Z"))
  {
    if (is_static)
      val.z = (*jni)->GetStaticBooleanField(jni, object, field_id->field_id);
    else
      val.z = (*jni)->GetBooleanField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushboolean(L, val.z);
  }
  else if (!strcmp(sig, "B"))
  {
    if (is_static)
      val.b = (*jni)->GetStaticByteField(jni, object, field_id->field_id);
    else
      val.b = (*jni)->GetByteField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.b);
  }
  else if (!strcmp(sig, "C"))
  {
    if (is_static)
      val.c = (*jni)->GetStaticCharField(jni, object, field_id->field_id);
    else
      val.c = (*jni)->GetCharField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.c);
  }
  else if (!strcmp(sig, "S"))
  {
    if (is_static)
      val.s = (*jni)->GetStaticShortField(jni, object, field_id->field_id);
    else
      val.s = (*jni)->GetShortField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.s);
  }
  else if (!strcmp(sig, "I"))
  {
    if (is_static)
      val.i = (*jni)->GetStaticIntField(jni, object, field_id->field_id);
    else
      val.i = (*jni)->GetIntField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.i);
  }
  else if (!strcmp(sig, "J"))
  {
    if (is_static)
      val.j = (*jni)->GetStaticLongField(jni, object, field_id->field_id);
    else
      val.j = (*jni)->GetLongField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.j);
  }
  else if (!strcmp(sig, "F"))
  {
    if (is_static)
      val.f = (*jni)->GetStaticFloatField(jni, object, field_id->field_id);
    else
      val.f = (*jni)->GetFloatField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushnumber(L, val.f);
  }
  else if (!strcmp(sig, "D"))
  {
    if (is_static)
      val.d = (*jni)->GetStaticDoubleField(jni, object, field_id->field_id);
    else
      val.d = (*jni)->GetDoubleField(jni, object, field_id->field_id);
    EXCEPTION_CHECK(jni);
    lua_pushnumber(L, val.d);
  }
  else
  {
    lj_print_message("Unknown return type '%s' for field\n", sig);
    lua_pushnil(L);
  }

  return 1;
}

static int lj_get_field_modifiers(lua_State *L)
{
  jint modifiers;
  lj_field_id *field_id;

  field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetFieldModifiers(lj_jvmti, field_id->class, field_id->field_id, &modifiers);
  lj_check_jvmti_error(L);

  lua_pushinteger(L, modifiers);

  return 1;
}

static int lj_get_field_modifiers_table(lua_State *L)
{
  lua_Integer modifiers;

  modifiers = luaL_checkinteger(L, 1);
  lua_pop(L, 1);

  /* do all this in C because there are no bitwise ops in Lua */
  lua_newtable(L);

  /* http://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.5 */
  lua_pushboolean(L, modifiers & JVM_ACC_PUBLIC);
  lua_setfield(L, -2, "public");
  lua_pushboolean(L, modifiers & JVM_ACC_PRIVATE);
  lua_setfield(L, -2, "private");
  lua_pushboolean(L, modifiers & JVM_ACC_PROTECTED);
  lua_setfield(L, -2, "protected");
  lua_pushboolean(L, modifiers & JVM_ACC_STATIC);
  lua_setfield(L, -2, "static");
  lua_pushboolean(L, modifiers & JVM_ACC_FINAL);
  lua_setfield(L, -2, "final");
  lua_pushboolean(L, modifiers & JVM_ACC_SYNCHRONIZED);
  lua_setfield(L, -2, "volatile");
  lua_pushboolean(L, modifiers & JVM_ACC_TRANSIENT);
  lua_setfield(L, -2, "transient");
  lua_pushboolean(L, modifiers & JVM_ACC_SYNTHETIC);
  lua_setfield(L, -2, "synthetic");
  lua_pushboolean(L, modifiers & JVM_ACC_ENUM);
  lua_setfield(L, -2, "enum");

  return 1;
}

static int lj_get_source_filename(lua_State *L)
{
  jobject class;
  char *sourcefile;

  class = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetSourceFileName(lj_jvmti, class, &sourcefile);
  if (lj_err == JVMTI_ERROR_ABSENT_INFORMATION)
  {
    lua_pushnil(L);
  }
  else
  {
    lj_check_jvmti_error(L);
    lua_pushstring(L, sourcefile);
  }

  return 1;
}

static int lj_get_line_number_table(lua_State *L)
{
  jmethodID method_id;
  jint line_count;
  jvmtiLineNumberEntry *lines;
  int i;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->GetLineNumberTable(lj_jvmti, method_id, &line_count, &lines);
  if (lj_err == JVMTI_ERROR_NATIVE_METHOD ||
      lj_err == JVMTI_ERROR_ABSENT_INFORMATION) {
    lua_pushnil(L);
  } else {
    lua_newtable(L);
    for (i = 0; i < line_count; ++i)
    {
      lua_newtable(L);
      lua_pushinteger(L, lines[i].start_location);
      lua_setfield(L, -2, "location");
      lua_pushinteger(L, lines[i].line_number);
      lua_setfield(L, -2, "line_num");
      lua_rawseti(L, -2, i+1);
    }
    lj_check_jvmti_error(L);
  }

  return 1;
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_pushinteger(L, location);
  lua_call(L, 3, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_call(L, 2, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_pushboolean(L, was_popped_by_exception);
  /* TODO return_value must be passed to Lua */
  lua_call(L, 3, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  new_jobject(L, thread);
  new_jmethod_id(L, method_id);
  lua_pushinteger(L, location);
  lua_call(L, 3, 1);
  lua_pop(lj_L, 1); /* the new lua_State, we're done with it */
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

static int lj_set_jvmti_callback(lua_State *L)
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
  if (!strcmp(callback, "single_step"))
  {
    EV_ENABLET(SINGLE_STEP, get_current_java_thread());
  }

  get_jvmti_callback_pointers(callback,
			      &jvmti_callback_ptr, &lj_callback_ptr, &ref_ptr);
  *jvmti_callback_ptr = lj_callback_ptr;
  *ref_ptr = ref;

  lj_err = (*lj_jvmti)->SetEventCallbacks(lj_jvmti, evCbs, sizeof(jvmtiEventCallbacks));
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_clear_jvmti_callback(lua_State *L)
{
  const char *callback;
  void **jvmti_callback_ptr;
  int *ref_ptr;
  jvmtiEventCallbacks *evCbs;

  callback = luaL_checkstring(L, 1);
  lua_pop(L, 1);

  evCbs = get_jvmti_callbacks();

  if (!strcmp(callback, "single_step"))
  {
    EV_DISABLET(SINGLE_STEP, get_current_java_thread());
  }

  get_jvmti_callback_pointers(callback, &jvmti_callback_ptr, NULL, &ref_ptr);
  *jvmti_callback_ptr = NULL;
  *ref_ptr = LUA_NOREF;

  lj_err = (*lj_jvmti)->SetEventCallbacks(lj_jvmti, evCbs, sizeof(jvmtiEventCallbacks));
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_get_current_thread(lua_State *L)
{
  jthread thread;

  /* JVMTI seems to be returning the agent thread, even though it says it won't */
  /* lj_err = (*lj_jvmti)->GetCurrentThread(lj_jvmti, &thread); */
  /* lj_check_jvmti_error(L); */
  thread = get_current_java_thread();

  new_jobject(L, thread);

  return 1;
}

static int lj_get_all_threads(lua_State *L)
{
  jint threads_count;
  jthread *threads;
  int i;

  lj_err = (*lj_jvmti)->GetAllThreads(lj_jvmti, &threads_count, &threads);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < threads_count; ++i)
  {
    new_jobject(L, threads[i]);
    lua_rawseti(L, -2, i+1);
  }

  free_jvmti_refs(lj_jvmti, threads, (void *)-1);

  return 1;
}

static int lj_create_raw_monitor(lua_State *L)
{
  jrawMonitorID monitor;
  const char *name;

  name = luaL_checkstring(L, 1);
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->CreateRawMonitor(lj_jvmti, name, &monitor);
  lj_check_jvmti_error(L);

  new_jmonitor(L, monitor, name);

  return 1;
}

static int lj_destroy_raw_monitor(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->DestroyRawMonitor(lj_jvmti, monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_enter(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->RawMonitorEnter(lj_jvmti, monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_exit(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->RawMonitorExit(lj_jvmti, monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_wait(lua_State *L)
{
  jrawMonitorID monitor;
  jlong wait;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor_mt");
  wait = luaL_checkint(L, 2);
  lua_pop(L, 2);

  lj_err = (*lj_jvmti)->RawMonitorWait(lj_jvmti, monitor, wait);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_notify(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->RawMonitorNotify(lj_jvmti, monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_raw_monitor_notify_all(lua_State *L)
{
  jrawMonitorID monitor;

  monitor = *(jrawMonitorID *)luaL_checkudata(L, 1, "jmonitor_mt");
  lua_pop(L, 1);

  lj_err = (*lj_jvmti)->RawMonitorNotifyAll(lj_jvmti, monitor);
  lj_check_jvmti_error(L);

  return 0;
}

static int lj_get_array_length(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jobject array = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  jsize length = (*jni)->GetArrayLength(jni, array);
  EXCEPTION_CHECK(jni);
  lua_pop(L, 1);
  lua_pushinteger(L, length);
  return 1;
}

static int lj_get_array_element(lua_State *L)
{
  JNIEnv *jni = current_jni();
  jobject array = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  const char *class_name = luaL_checkstring(L, 2);
  jsize index = luaL_checkinteger(L, 3) - 1;
  jvalue val;
  lua_pop(L, 3);

  assert(*class_name == '[');
  switch (class_name[1])
  {
  case 'L':
  case '[':
    val.l = (*jni)->GetObjectArrayElement(jni, array, index);
    EXCEPTION_CHECK(jni);
    new_jobject(L, val.l);
    break;
  case 'Z':
    (*jni)->GetBooleanArrayRegion(jni, array, index, 1, &val.z);
    EXCEPTION_CHECK(jni);
    lua_pushboolean(L, val.z);
    break;
  case 'B':
    (*jni)->GetByteArrayRegion(jni, array, index, 1, &val.b);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.b);
    break;
  case 'C':
    (*jni)->GetCharArrayRegion(jni, array, index, 1, &val.c);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.c);
    break;
  case 'S':
    (*jni)->GetShortArrayRegion(jni, array, index, 1, &val.s);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.s);
    break;
  case 'I':
    (*jni)->GetIntArrayRegion(jni, array, index, 1, &val.i);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.i);
    break;
  case 'J':
    (*jni)->GetLongArrayRegion(jni, array, index, 1, &val.j);
    EXCEPTION_CHECK(jni);
    lua_pushinteger(L, val.j);
    break;
  case 'F':
    (*jni)->GetFloatArrayRegion(jni, array, index, 1, &val.f);
    EXCEPTION_CHECK(jni);
    lua_pushnumber(L, val.f);
    break;
  case 'D':
    (*jni)->GetDoubleArrayRegion(jni, array, index, 1, &val.d);
    EXCEPTION_CHECK(jni);
    lua_pushnumber(L, val.d);
    break;
  default:
    (void)luaL_error(L, "Unknown array class: %s\n", class_name);
  }

  return 1;
}

 /*           _____ _____  */
 /*     /\   |  __ \_   _| */
 /*    /  \  | |__) || |   */
 /*   / /\ \ |  ___/ | |   */
 /*  / ____ \| |    _| |_  */
 /* /_/    \_\_|   |_____| *//* in more ways than one... */

/* Lua API -> */

void lj_init(lua_State *L, JavaVM *jvm, jvmtiEnv *jvmti)
{
  lj_L = L;

  /* add C functions */
  lua_register(L, "lj_get_frame_count",            lj_get_frame_count);
  lua_register(L, "lj_get_stack_frame",            lj_get_stack_frame);
  lua_register(L, "lj_set_breakpoint",             lj_set_breakpoint);
  lua_register(L, "lj_clear_breakpoint",           lj_clear_breakpoint);
  lua_register(L, "lj_get_local_variable_table",   lj_get_local_variable_table);
  lua_register(L, "lj_get_method_id",              lj_get_method_id);
  lua_register(L, "lj_get_local_variable",         lj_get_local_variable);
  lua_register(L, "lj_pointer_to_string",          lj_pointer_to_string);
  lua_register(L, "lj_get_class_methods",          lj_get_class_methods);
  lua_register(L, "lj_find_class",                 lj_find_class);
  lua_register(L, "lj_call_method",                lj_call_method);
  lua_register(L, "lj_toString",                   lj_toString);
  lua_register(L, "lj_get_method_name",            lj_get_method_name);
  lua_register(L, "lj_get_method_declaring_class", lj_get_method_declaring_class);
  lua_register(L, "lj_get_method_modifiers",       lj_get_method_modifiers);
  lua_register(L, "lj_get_method_modifiers_table", lj_get_method_modifiers_table);
  lua_register(L, "lj_get_field_name",             lj_get_field_name);
  lua_register(L, "lj_get_field_declaring_class",  lj_get_field_declaring_class);
  lua_register(L, "lj_get_field_id",               lj_get_field_id);
  lua_register(L, "lj_get_class_fields",           lj_get_class_fields);
  lua_register(L, "lj_get_field",                  lj_get_field);
  lua_register(L, "lj_get_field_modifiers",        lj_get_field_modifiers);
  lua_register(L, "lj_get_field_modifiers_table",  lj_get_field_modifiers_table);
  lua_register(L, "lj_get_source_filename",        lj_get_source_filename);
  lua_register(L, "lj_get_line_number_table",      lj_get_line_number_table);

  lua_register(L, "lj_set_jvmti_callback",         lj_set_jvmti_callback);
  lua_register(L, "lj_clear_jvmti_callback",       lj_clear_jvmti_callback);

  lua_register(L, "lj_get_current_thread",         lj_get_current_thread);
  lua_register(L, "lj_get_all_threads",            lj_get_all_threads);

  /* raw monitor */
  lua_register(L, "lj_create_raw_monitor",         lj_create_raw_monitor);
  lua_register(L, "lj_destroy_raw_monitor",        lj_destroy_raw_monitor);
  lua_register(L, "lj_raw_monitor_enter",          lj_raw_monitor_enter);
  lua_register(L, "lj_raw_monitor_exit",           lj_raw_monitor_exit);
  lua_register(L, "lj_raw_monitor_wait",           lj_raw_monitor_wait);
  lua_register(L, "lj_raw_monitor_notify",         lj_raw_monitor_notify);
  lua_register(L, "lj_raw_monitor_notify_all",     lj_raw_monitor_notify_all);

  lua_register(L, "lj_get_array_length",           lj_get_array_length);
  lua_register(L, "lj_get_array_element",          lj_get_array_element);

  /* clear callback refs */
  lj_jvmti_callbacks.cb_breakpoint_ref = LUA_NOREF;
  lj_jvmti_callbacks.cb_method_entry_ref = LUA_NOREF;
  lj_jvmti_callbacks.cb_method_exit_ref = LUA_NOREF;
  lj_jvmti_callbacks.cb_single_step_ref = LUA_NOREF;

  /* save pointers for global use */
  lj_jvm = jvm;
  lj_jvmti = jvmti;
}

void lj_print_message(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  fflush(stdout);
  va_end(ap);
}
