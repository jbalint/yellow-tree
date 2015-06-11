#include "myjni.h"
#include "jni_util.h"
#include "lua_interface.h"
#include "lua_java.h"
#include "java_bridge.h"
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
  jfieldID *fields = NULL;
  jclass class;
  int i;

  class = *(jclass *)luaL_checkudata(L, 1, "jobject");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetClassFields(current_jvmti(), class, &field_count, &fields);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < field_count; ++i)
  {
    new_jfield_id(L, fields[i], class);
    lua_rawseti(L, -2, i+1);
  }

  if (fields)
	free_jvmti_refs(current_jvmti(), fields, (void *)-1);

  return 1;
}

static int lj_get_class_methods(lua_State *L)
{
  jint method_count;
  jmethodID *methods = NULL;
  jclass class;
  int i;

  class = *(jclass *)luaL_checkudata(L, 1, "jobject");
  lua_pop(L, 1);

  lj_err = (*current_jvmti())->GetClassMethods(current_jvmti(), class, &method_count, &methods);
  lj_check_jvmti_error(L);

  lua_newtable(L);

  for (i = 0; i < method_count; ++i)
  {
    new_jmethod_id(L, methods[i]);
    lua_rawseti(L, -2, i+1);
  }

  if (methods)
	free_jvmti_refs(current_jvmti(), methods, (void *)-1);

  return 1;
}

static int lj_get_source_filename(lua_State *L)
{
  jobject class;
  char *sourcefile = NULL;

  class = *(jobject *)luaL_checkudata(L, 1, "jobject");
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

	free_jvmti_refs(current_jvmti(), sourcefile, (void *)-1);
  }

  return 1;
}

static int lj_get_loaded_classes(lua_State *L)
{
  JNIEnv *jni = current_jni();
  char *class_sig;
  jclass *classes;
  jint class_count;
  int i;

  lj_err = (*current_jvmti())->GetLoadedClasses(current_jvmti(), &class_count, &classes);
  lj_check_jvmti_error(L);

  fprintf(stderr, "lj_get_loaded_classes: (Loading %d classes)\n", class_count);

  lua_createtable(L, 0, class_count);

  /* create a table indexed with *internal* class name */
  for (i = 0; i < class_count; ++i)
  {
	lj_err = (*current_jvmti())->GetClassSignature(current_jvmti(), classes[i], &class_sig, NULL);
	lj_check_jvmti_error(L);
	lua_pushstring(L, class_sig);
    new_jobject(L, (*jni)->NewGlobalRef(jni, classes[i]));
	/* newtable[class_sig] = classes[i] */
    lua_rawset(L, -3);
	free_jvmti_refs(current_jvmti(), class_sig, (void *) -1);
  }

  if (classes)
	free_jvmti_refs(current_jvmti(), classes, (void *) -1);

  return 1;
}

void lj_class_register(lua_State *L)
{
  lua_register(L, "lj_find_class",                 lj_find_class);
  lua_register(L, "lj_get_class_fields",           lj_get_class_fields);
  lua_register(L, "lj_get_class_methods",          lj_get_class_methods);
  lua_register(L, "lj_get_source_filename",        lj_get_source_filename);
  lua_register(L, "lj_get_loaded_classes",         lj_get_loaded_classes);
}
