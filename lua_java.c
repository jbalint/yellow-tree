#include <assert.h>

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
static JNIEnv *lj_jni;

/* marker where NULL is used as a jthread param for current thread */
#define NULL_JTHREAD NULL

static void lj_check_jvmti_error(lua_State *L)
{
  char *errmsg = "<Unknown Error>";
  if(lj_err == JVMTI_ERROR_NONE)
    return;

  fprintf(stderr, "Error %d from JVMTI", lj_err);
  if((*lj_jvmti)->GetErrorName(lj_jvmti, lj_err, &errmsg) == JVMTI_ERROR_NONE)
  {
    fprintf(stderr, ": %s", errmsg);
    (*lj_jvmti)->Deallocate(lj_jvmti, (unsigned char *)errmsg);
  }
  fprintf(stderr, "\n");

  if (IsDebuggerPresent())
    DebugBreak();

  (void)luaL_error(L, "Error %d from JVMTI: %s\n", lj_err, errmsg);
}

/* allocate a new userdata object for a jthread */
static void new_jthread(lua_State *L, jthread thread)
{
  jthread *user_data;
  user_data = lua_newuserdata(L, sizeof(jthread));
  *user_data = thread;
  lua_getglobal(L, "jthread_mt");
  lua_setmetatable(L, -2);
}

/* allocate a new userdata object for a jmethodID */
static void new_jmethod_id(lua_State *L, jmethodID method_id)
{
  jmethodID *user_data;
  user_data = lua_newuserdata(L, sizeof(jmethodID));
  *user_data = method_id;
  lua_getglobal(L, "jmethod_id_mt");
  lua_setmetatable(L, -2);
}

static void new_jobject(lua_State *L, jobject object)
{
  jobject *user_data;
  user_data = lua_newuserdata(L, sizeof(jobject));
  *user_data = object;
  lua_getglobal(L, "jobject_mt");
  lua_setmetatable(L, -2);
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
  lj_err = (*lj_jvmti)->GetFrameCount(lj_jvmti, NULL_JTHREAD, &count);
  lj_check_jvmti_error(L);
  lua_pushinteger(L, count);
  return 1;
}

static int lj_get_stack_frame(lua_State *L)
{
  int frame_num;
  jvmtiFrameInfo fi;
  jint count;
  jclass cls;
  char *class_name;
  char *sourcefile = NULL;
  char *method_name;
  char *source = "<file>";
  int j;
  jint line_number;
  jint linescount;
  jvmtiLineNumberEntry *lines;

  frame_num = luaL_checkint(L, 1);
  lua_pop(L, 1);

  /* get stack frame info */
  lj_err = (*lj_jvmti)->GetStackTrace(lj_jvmti, NULL_JTHREAD, frame_num, 1, &fi, &count);
  lj_check_jvmti_error(L);
  if (count == 0) {
    return 0;
  }
  assert(count == 1);

  lj_err = (*lj_jvmti)->GetMethodDeclaringClass(lj_jvmti, fi.method, &cls);
  lj_check_jvmti_error(L);
  assert(cls != NULL);

  lj_err = (*lj_jvmti)->GetClassSignature(lj_jvmti, cls, &class_name, NULL);
  lj_check_jvmti_error(L);
  lj_err = (*lj_jvmti)->GetMethodName(lj_jvmti, fi.method, &method_name, NULL, NULL);
  lj_check_jvmti_error(L);
  lj_err = (*lj_jvmti)->GetSourceFileName(lj_jvmti, cls, &sourcefile);
  lj_check_jvmti_error(L);

  lj_err = (*lj_jvmti)->GetLineNumberTable(lj_jvmti, fi.method, &linescount, &lines);
  if(lj_err == JVMTI_ERROR_NATIVE_METHOD)
    source = "<native_method>";
  else if(lj_err == JVMTI_ERROR_ABSENT_INFORMATION)
    source = "<absent_info>";
  else
    lj_check_jvmti_error(L);

  /* link the bytecode offset to LineNumberTable if exists */
  for (j = 0; j < linescount; ++j)
  {
    if(j < (linescount - 1) &&
       fi.location < lines[j+1].start_location)
      break;
    else if(j == (linescount - 1))
      break;
  }
  if(linescount)
    line_number = lines[j].line_number;

  lua_newtable(L);

  lua_pushstring(L, class_name);
  lua_setfield(L, -2, "class");

  lua_pushstring(L, method_name);
  lua_setfield(L, -2, "method");

  lua_pushstring(L, source);
  lua_setfield(L, -2, "source");

  lua_pushinteger(L, line_number);
  lua_setfield(L, -2, "line_num");

  if (sourcefile)
  {
    lua_pushstring(L, sourcefile);
    lua_setfield(L, -2, "sourcefile");
  }

  new_jmethod_id(L, fi.method);
  lua_setfield(L, -2, "method_id");

  free_jvmti_refs(lj_jvmti, method_name, class_name, sourcefile, lines, (void *)-1);

  return 1;
}

static int lj_set_breakpoint(lua_State *L)
{
  jmethodID method_id;
  int line_num = 0;
  jint bytecode_index = 0;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  line_num = luaL_checkinteger(L, 2);
  lua_pop(L, 2);

  bytecode_index = method_find_line_bytecode_index(lj_jvmti, method_id, line_num);

  lj_err = (*lj_jvmti)->SetBreakpoint(lj_jvmti, method_id, bytecode_index);
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

static int lj_get_current_thread(lua_State *L)
{
  jthread thread;

  lj_err = (*lj_jvmti)->GetCurrentThread(lj_jvmti, &thread);
  lj_check_jvmti_error(L);

  new_jthread(L, thread);

  return 1;
}

static int lj_get_method_id(lua_State *L)
{
  jvmtiError jerr;
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
  class = (*lj_jni)->FindClass(lj_jni, class_name);
  EXCEPTION_CLEAR(lj_jni);
  if (class == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  /* build signature string */
  sig = malloc(strlen(args) + strlen(ret) + 10);
  sprintf(sig, "(%s)%s", args, ret);

  /* try instance method */
  method_id = (*lj_jni)->GetMethodID(lj_jni, class, method_name, sig);
  EXCEPTION_CLEAR(lj_jni);

  /* otherwise try static method */
  if (method_id == NULL)
    method_id = (*lj_jni)->GetStaticMethodID(lj_jni, class, method_name, sig);
  EXCEPTION_CLEAR(lj_jni);
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
  jobject val_o;

  depth = luaL_checkinteger(L, 1);
  slot = luaL_checkinteger(L, 2);
  type = luaL_checkstring(L, 3);
  lua_pop(L, 3);

  if (!strcmp(type, "I")) {
    lj_err = (*lj_jvmti)->GetLocalInt(lj_jvmti, NULL_JTHREAD, depth, slot, &val_i);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushinteger(L, val_i);
    }
  } else if (!strcmp(type, "J")) {
    lj_err = (*lj_jvmti)->GetLocalLong(lj_jvmti, NULL_JTHREAD, depth, slot, &val_j);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushinteger(L, val_j);
    }
  } else if (!strcmp(type, "F")) {
    lj_err = (*lj_jvmti)->GetLocalFloat(lj_jvmti, NULL_JTHREAD, depth, slot, &val_f);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushnumber(L, val_f);
    }
  } else if (!strcmp(type, "D")) {
    lj_err = (*lj_jvmti)->GetLocalDouble(lj_jvmti, NULL_JTHREAD, depth, slot, &val_d);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      lua_pushnumber(L, val_d);
    }
  } else if (!strcmp(type, "O")) { /* non-spec indicator of a jobject */
    /* GetLocalInstance() is new to JVMTI 1.2 */
    /* if (slot == 0) */
    /*   lj_err = (*lj_jvmti)->GetLocalInstance(lj_jvmti, NULL_JTHREAD, depth, &val_o); */
    /* else */
      lj_err = (*lj_jvmti)->GetLocalObject(lj_jvmti, NULL_JTHREAD, depth, slot, &val_o);
    if (local_variable_is_nil(lj_err))
    {
      lua_pushnil(L);
    }
    else
    {
      lj_check_jvmti_error(L);
      new_jobject(L, val_o);
    }
  } else {
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
  sprintf(buf, "%p", p);
  lua_pushstring(L, buf);
  return 1;
}

 /*           _____ _____  */
 /*     /\   |  __ \_   _| */
 /*    /  \  | |__) || |   */
 /*   / /\ \ |  ___/ | |   */
 /*  / ____ \| |    _| |_  */
 /* /_/    \_\_|   |_____| *//* in more ways than one... */

void lj_init(lua_State *L, jvmtiEnv *jvmti)
{
  /* add C functions */
  lua_register(L, "lj_get_frame_count",          lj_get_frame_count);
  lua_register(L, "lj_get_stack_frame",          lj_get_stack_frame);
  lua_register(L, "lj_set_breakpoint",           lj_set_breakpoint);
  lua_register(L, "lj_get_local_variable_table", lj_get_local_variable_table);
  lua_register(L, "lj_get_current_thread",       lj_get_current_thread);
  lua_register(L, "lj_get_method_id",            lj_get_method_id);
  lua_register(L, "lj_get_local_variable",       lj_get_local_variable);
  lua_register(L, "lj_pointer_to_string",        lj_pointer_to_string);

  /* add Java type metatables to registry for luaL_checkdata() convenience */
  lua_getglobal(L, "jthread_mt");
  lua_setfield(L, LUA_REGISTRYINDEX, "jthread_mt");
  lua_getglobal(L, "jmethod_id_mt");
  lua_setfield(L, LUA_REGISTRYINDEX, "jmethod_id_mt");

  /* save jvmtiEnv pointer for global use */
  lj_jvmti = jvmti;
}

void lj_set_jni(JNIEnv *jni)
{
  lj_jni = jni;
}
