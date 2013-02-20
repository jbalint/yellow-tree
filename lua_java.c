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

static void new_string(lua_State *L, jstring string)
{
  const jbyte *utf_chars;
  utf_chars = (*lj_jni)->GetStringUTFChars(lj_jni, string, NULL);
  if (utf_chars)
  {
    lua_pushstring(L, utf_chars);
    (*lj_jni)->ReleaseStringUTFChars(lj_jni, string, utf_chars);
    EXCEPTION_CHECK(lj_jni);
  }
  else
  {
    lua_pushnil(L);
  }
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
  char *sourcefile;
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

  lua_pushinteger(L, frame_num);
  lua_setfield(L, -2, "depth");

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

  new_jobject(L, thread);

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
  jobject val_l;

  depth = luaL_checkinteger(L, 1);
  slot = luaL_checkinteger(L, 2);
  type = luaL_checkstring(L, 3);
  lua_pop(L, 3);

  if (!strcmp(type, "I"))
  {
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
  }
  else if (!strcmp(type, "J"))
  {
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
  }
  else if (!strcmp(type, "F"))
  {
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
  }
  else if (!strcmp(type, "D"))
  {
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
  }
  else if (!strcmp(type, "L"))
  {
    /* GetLocalInstance() is new to JVMTI 1.2 */
    /* if (slot == 0) */
    /*   lj_err = (*lj_jvmti)->GetLocalInstance(lj_jvmti, NULL_JTHREAD, depth, &val_l); */
    /* else */
      lj_err = (*lj_jvmti)->GetLocalObject(lj_jvmti, NULL_JTHREAD, depth, slot, &val_l);
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
  sprintf(buf, "%p", p);
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
  jclass class;
  const char *class_name;

  class_name = luaL_checkstring(L, 1);
  lua_pop(L, 1);

  class = (*lj_jni)->FindClass(lj_jni, class_name);
  EXCEPTION_CHECK(lj_jni);
  if (class)
    new_jobject(L, class);
  else
    lua_pushnil(L);

  return 1;
}

/* first prototype of generic method calling */
static int lj_call_method(lua_State *L)
{
  jobject object;
  jmethodID method_id;
  const char *args;
  const char *ret;
  jobject val_l;
  int argcount;
  int i;
  int param_num;
  int result_count;

  jvalue *jargs;

  const char *argtype;

  jint jarg_i;
  jlong jarg_j;
  jfloat jarg_f;
  jdouble jarg_d;
  jobject jarg_l;

  object = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  method_id = *(jmethodID *)luaL_checkudata(L, 2, "jmethod_id_mt");
  ret = luaL_checkstring(L, 3);
  argcount = luaL_checkinteger(L, 4);

  if (!strcmp("V", ret))
    result_count = 0;
  else
    result_count = 1;

  jargs = malloc(sizeof(jvalue) * argcount);

  param_num = 5;
  /* get arguments */
  for (i = 0; i < argcount; ++i)
  {
    argtype = luaL_checkstring(L, param_num++);
    if (!strcmp("L", argtype))
    {
      jargs[i].l = luaL_checkudata(L, param_num++, "jobject_mt");
    }
    else if (!strcmp("STR", argtype)) /* TODO non-standard indicator */
    {
      jargs[i].l = (*lj_jni)->NewStringUTF(lj_jni, luaL_checkstring(L, param_num++));
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

  lua_pop(L, argcount + 4);

  /* call method */
  if (!strcmp("L", ret) || !strcmp("STR", ret))
  {
    val_l = (*lj_jni)->CallObjectMethodA(lj_jni, object, method_id, jargs);
    EXCEPTION_CHECK(lj_jni);
    if (val_l)
    {
      if (!strcmp("L", ret))
	new_jobject(L, val_l);
      else
	new_string(L, val_l);
    }
    else
    {
      lua_pushnil(L);
    }
  }
  else if (!strcmp("V", ret))
  {
    (*lj_jni)->CallVoidMethodA(lj_jni, object, method_id, jargs);
  }
  else
  {
    /* to keep lua_happy until other return types are implemented */
    lua_pushnil(L);
  }

  return result_count;
}

static int lj_toString(lua_State *L)
{
  jobject object;
  jclass class;
  jmethodID method_id;
  jstring string;

  object = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  class = (*lj_jni)->FindClass(lj_jni, "java/lang/Object");
  EXCEPTION_CHECK(lj_jni);
  if (class == NULL)
  {
    lua_pushnil(L);
    return 1;
  }
  method_id = (*lj_jni)->GetMethodID(lj_jni, class, "toString", "()Ljava/lang/String;");
  EXCEPTION_CHECK(lj_jni);
  if (method_id == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  string = (jstring)(*lj_jni)->CallObjectMethod(lj_jni, object, method_id);
  EXCEPTION_CHECK(lj_jni);
  if (string == NULL)
  {
    lua_pushnil(L);
    return 1;
  }

  new_string(L, string);

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

 /*           _____ _____  */
 /*     /\   |  __ \_   _| */
 /*    /  \  | |__) || |   */
 /*   / /\ \ |  ___/ | |   */
 /*  / ____ \| |    _| |_  */
 /* /_/    \_\_|   |_____| *//* in more ways than one... */

/* Lua API -> */
  /* int lj_get_frame_count()
     - return number of stack frames in current thread */
  /* {class, method, source, line_num, sourcefile, method_id, depth}
         lj_get_stack_frame(int frame_num)
     - get a stack frame in current thread */
  /* void lj_set_breakpoint(jmethod_id method_id, int lin_num)
     - set a breakpoint */
  /* [{name, sig, start_location, slot}] lj_get_local_variable_table(jmethod_id method_id)
     - get the local variable table for the specified method */
  /* jthread lj_get_current_thread()
     - get a reference to the current thread */
  /* jmethod_id lj_get_method_id(char *class_name, char *method_name, char *args, char *ret)
     - get a method_id */
  /* X lj_get_local_variable(int depth, int slot, char *type)
     - get value of a local variable */
  /* char *lj_point_to_string(void *user_data)
     - create a string from a user_data pointer */
  /* [jmethod_id] lj_get_class_methods(jclass class)
     - get methods declared on given class */
  /* jclass lj_find_class(char *class_name)
     - find a class given string in format pkg/Class */
  /* lj_call_method(...) */
  /* char *lj_toString(jobject object)
     - call object.toString() */
  /* {name, sig} lj_get_method_name(jmethod_id method_id)
     - get method name and signature */
  /* jclass lj_get_method_declaring_class(jmethod_id method_id)
     - get class that declares the given method */

void lj_init(lua_State *L, jvmtiEnv *jvmti)
{
  /* add C functions */
  lua_register(L, "lj_get_frame_count",            lj_get_frame_count);
  lua_register(L, "lj_get_stack_frame",            lj_get_stack_frame);
  lua_register(L, "lj_set_breakpoint",             lj_set_breakpoint);
  lua_register(L, "lj_get_local_variable_table",   lj_get_local_variable_table);
  lua_register(L, "lj_get_current_thread",         lj_get_current_thread);
  lua_register(L, "lj_get_method_id",              lj_get_method_id);
  lua_register(L, "lj_get_local_variable",         lj_get_local_variable);
  lua_register(L, "lj_pointer_to_string",          lj_pointer_to_string);
  lua_register(L, "lj_get_class_methods",          lj_get_class_methods);
  lua_register(L, "lj_find_class",                 lj_find_class);
  lua_register(L, "lj_call_method",                lj_call_method);
  lua_register(L, "lj_toString",                   lj_toString);
  lua_register(L, "lj_get_method_name",            lj_get_method_name);
  lua_register(L, "lj_get_method_declaring_class", lj_get_method_declaring_class);

  /* add Java type metatables to registry for luaL_checkdata() convenience */
  lua_getglobal(L, "jmethod_id_mt");
  lua_setfield(L, LUA_REGISTRYINDEX, "jmethod_id_mt");
  lua_getglobal(L, "jobject_mt");
  lua_setfield(L, LUA_REGISTRYINDEX, "jobject_mt");

  /* save jvmtiEnv pointer for global use */
  lj_jvmti = jvmti;
}

void lj_set_jni(JNIEnv *jni)
{
  lj_jni = jni;
}
