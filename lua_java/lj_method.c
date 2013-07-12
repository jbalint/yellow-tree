#include <stdlib.h>
#include <string.h>
#include <classfile_constants.h>

#include "myjni.h"
#include "jni_util.h"
#include "lua_interface.h"
#include "lua_java.h"
#include "lj_internal.h"

/* Lua wrappers for method operations */

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

static int lj_get_local_variable_table(lua_State *L)
{
  jmethodID method_id;
  jvmtiLocalVariableEntry *vars;
  jint count;
  int i;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetLocalVariableTable(current_jvmti(), method_id, &count, &vars);
  if (lj_err == JVMTI_ERROR_ABSENT_INFORMATION ||
	  lj_err == JVMTI_ERROR_NATIVE_METHOD)
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

  free_jvmti_refs(current_jvmti(), vars, (void *)-1);

  return 1;
}

static int lj_get_method_name(lua_State *L)
{
  jmethodID method_id;
  char *method_name;
  char *sig;

  method_id = *(jmethodID *)luaL_checkudata(L, 1, "jmethod_id_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetMethodName(current_jvmti(), method_id, &method_name, &sig, NULL);
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

  lj_err = (*current_jvmti())->GetMethodDeclaringClass(current_jvmti(), method_id, &class);
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

  lj_err = (*current_jvmti())->GetMethodModifiers(current_jvmti(), method_id, &modifiers);
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

void lj_method_register(lua_State *L)
{
  lua_register(L, "lj_get_method_id",              lj_get_method_id);
  lua_register(L, "lj_get_local_variable_table",   lj_get_local_variable_table);
  lua_register(L, "lj_get_method_name",            lj_get_method_name);
  lua_register(L, "lj_get_method_declaring_class", lj_get_method_declaring_class);
  lua_register(L, "lj_get_method_modifiers",       lj_get_method_modifiers);
  lua_register(L, "lj_get_method_modifiers_table", lj_get_method_modifiers_table);
}
