#include <assert.h>

#include "myjni.h"
#include "jni_util.h"
#include "lua_java.h"

static jvmtiEnv *lj_jvmti;
static jvmtiError lj_err;
static JNIEnv *lj_jni;

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

static int lj_get_frame_count(lua_State *L)
{
  jint count;
  lj_err = (*lj_jvmti)->GetFrameCount(lj_jvmti, NULL, &count);
  lj_check_jvmti_error(L);
  lua_pushinteger(L, count);
  return 1; /* one result */
}

static int lj_get_stack_frame(lua_State *L)
{
  int frame_num = luaL_checkint(L, 1);
  jvmtiFrameInfo fi;
  jint count;
  jclass cls;
  char *class_name;
  char *sourcefile;
  char *method_name;

  /* get stack frame info */
  lj_err = (*lj_jvmti)->GetStackTrace(lj_jvmti, NULL, frame_num, 1, &fi, &count);
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

  /* Gagent.jerr = (*Gagent.jvmti)->GetLineNumberTable(Gagent.jvmti, frames[i].method, */
  /* 					    &linescount, &lines); */
  /*   if(Gagent.jerr == JVMTI_ERROR_NATIVE_METHOD) */
  /*   { */
  /*     sourcefile = "<native_method>"; */
  /*     assert(!freesf); */
  /*   } */
  /*   else if(Gagent.jerr != JVMTI_ERROR_ABSENT_INFORMATION) */
  /*     check_jvmti_error(Gagent.jvmti, Gagent.jerr); */
    /* link the bytecode offset to LineNumberTable if exists */
    /* for(j = 0; j < linescount; ++j) */
    /* { */
    /*   if(j < (linescount - 1) && */
    /* 		 frames[i].location < lines[j+1].start_location) */
    /* 		break; */
    /*   else if(j == (linescount - 1)) */
    /* 		break; */
    /* } */
    /* if(linescount) */
    /*   line_number = lines[j].line_number; */

  lua_newtable(L);
  lua_pushstring(L, "class");
  lua_pushstring(L, class_name);
  lua_settable(L, -3);
  lua_pushstring(L, "method");
  lua_pushstring(L, method_name);
  lua_settable(L, -3);
  lua_pushstring(L, "sourcefile");
  lua_pushstring(L, sourcefile);
  lua_settable(L, -3);

  free_jvmti_refs(lj_jvmti, method_name, class_name, (void *)-1);
  if(sourcefile)
    (*lj_jvmti)->Deallocate(lj_jvmti, (unsigned char *)sourcefile);

  return 1;
}

static int lj_set_breakpoint(lua_State *L)
{
  const char *class;
  const char *method;
  const char *args;
  const char *ret;
  jmethodID method_id;
  int line_num = 0;
  jint bytecode_index = 0;

  line_num = lua_tointeger(L, -1);
  lua_pop(L, 1);

  lua_pushstring(L, "class");
  lua_gettable(L, -2);
  class = lua_tostring(L, -1);
  lua_pop(L, 1);

  lua_pushstring(L, "method");
  lua_gettable(L, -2);
  method = lua_tostring(L, -1);
  lua_pop(L, 1);

  lua_pushstring(L, "args");
  lua_gettable(L, -2);
  args = lua_tostring(L, -1);
  lua_pop(L, 1);

  lua_pushstring(L, "ret");
  lua_gettable(L, -2);
  ret = lua_tostring(L, -1);
  lua_pop(L, 1);

  method_id = method_decl_lookup(lj_jvmti, lj_jni, class, method, args, ret);
  if (method_id == NULL)
    (void)luaL_error(L, "Could not find method %s.%s(%s)%s", class, method, args, ret);
  bytecode_index = method_find_line_bytecode_index(lj_jvmti, method_id, line_num);

  lj_err = (*lj_jvmti)->SetBreakpoint(lj_jvmti, method_id, bytecode_index);
  lj_check_jvmti_error(L);

  return 0;
}

void lj_init(lua_State *L, jvmtiEnv *jvmti)
{
  lua_register(L, "lj_get_frame_count", lj_get_frame_count);
  lua_register(L, "lj_get_stack_frame", lj_get_stack_frame);
  lua_register(L, "lj_set_breakpoint", lj_set_breakpoint);
  lj_jvmti = jvmti;
}

void lj_set_jni(JNIEnv *jni)
{
  lj_jni = jni;
}
