#include "myjni.h"
#include "jni_util.h"
#include "lua_interface.h"
#include "lua_java.h"
#include "lj_internal.h"

/* Lua wrappers for class operations */

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
	/* TODO verify the exception to be pedantic and catch other errors */
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

static int lj_get_class_fields(lua_State *L)
{
  jint field_count;
  jfieldID *fields;
  jclass class;
  int i;

  class = *(jclass *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetClassFields(current_jvmti(), class, &field_count, &fields);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < field_count; ++i)
  {
    new_jfield_id(L, fields[i], class);
    lua_rawseti(L, -2, i+1);
  }

  free_jvmti_refs(current_jvmti(), fields, (void *)-1);

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

  lj_err = (*current_jvmti())->GetClassMethods(current_jvmti(), class, &method_count, &methods);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < method_count; ++i)
  {
    new_jmethod_id(L, methods[i]);
    lua_rawseti(L, -2, i+1);
  }

  free_jvmti_refs(current_jvmti(), methods, (void *)-1);

  return 1;
}

static int lj_get_source_filename(lua_State *L)
{
  jobject class;
  char *sourcefile;

  class = *(jobject *)luaL_checkudata(L, 1, "jobject_mt");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetSourceFileName(current_jvmti(), class, &sourcefile);
  if (lj_err == JVMTI_ERROR_ABSENT_INFORMATION)
  {
    lua_pushnil(L);
  }
  else
  {
    lj_check_jvmti_error(L);
    lua_pushstring(L, sourcefile);
  }

  free_jvmti_refs(current_jvmti(), sourcefile, (void *)-1);

  return 1;
}

void lj_class_register(lua_State *L)
{
  lua_register(L, "lj_find_class",                 lj_find_class);
  lua_register(L, "lj_get_class_fields",           lj_get_class_fields);
  lua_register(L, "lj_get_class_methods",          lj_get_class_methods);
  lua_register(L, "lj_get_source_filename",        lj_get_source_filename);
}
