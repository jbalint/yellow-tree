#include <classfile_constants.h>

#include "myjni.h"
#include "jni_util.h"
#include "lua_interface.h"
#include "lua_java.h"
#include "lj_internal.h"

/* Lua wrappers for field operations */

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

static int lj_get_field_name(lua_State *L)
{
  lj_field_id *field_id;
  char *field_name = NULL;
  char *sig = NULL;

  field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetFieldName(current_jvmti(), field_id->class, field_id->field_id, &field_name, &sig, NULL);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  lua_pushstring(L, field_name);
  lua_setfield(L, -2, "name");
  lua_pushstring(L, sig);
  lua_setfield(L, -2, "sig");

  free_jvmti_refs(current_jvmti(), field_name, sig, (void *)-1);

  return 1;
}

static int lj_get_field_declaring_class(lua_State *L)
{
  lj_field_id *field_id;
  jclass class;

  field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetFieldDeclaringClass(current_jvmti(), field_id->class, field_id->field_id, &class);
  lj_check_jvmti_error(L);

  new_jobject(L, class);

  return 1;
}

static int lj_get_field_modifiers(lua_State *L)
{
  jint modifiers;
  lj_field_id *field_id;

  field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetFieldModifiers(current_jvmti(), field_id->class, field_id->field_id, &modifiers);
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

void lj_field_register(lua_State *L)
{
  lua_register(L, "lj_get_field_id",               lj_get_field_id);
  lua_register(L, "lj_get_field_name",             lj_get_field_name);
  lua_register(L, "lj_get_field_declaring_class",  lj_get_field_declaring_class);
  lua_register(L, "lj_get_field_modifiers",        lj_get_field_modifiers);
  lua_register(L, "lj_get_field_modifiers_table",  lj_get_field_modifiers_table);
}
