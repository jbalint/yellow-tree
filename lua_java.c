#include "myjni.h"
#include "lua_java.h"

static jvmtiEnv *lj_jvmti;
static jvmtiError lj_err;

static void lj_check_jvmti_error(lua_State *L) {
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

  (void)luaL_error(L, "Error %d from JVMTI: %s\n", lj_err, errmsg);
}

void lj_init(lua_State *L, jvmtiEnv *jvmti) {
  lua_register(L, "lj_get_frame_count", lj_get_frame_count);
  lj_jvmti = jvmti;
}

int lj_get_frame_count(lua_State *L) {
  jint count;
  lj_err = (*lj_jvmti)->GetFrameCount(lj_jvmti, NULL, &count);
  lj_check_jvmti_error(L);
  lua_pushinteger(L, count);
  return 1; /* one result */
}
