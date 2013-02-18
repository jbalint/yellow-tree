#include "myjni.h"
#include "lua_java.h"

void lj_init(lua_State *L, jvmtiEnv *jvmti) {
  lua_pushcfunction(L, lj_get_frame_count);
  lua_setglobal(L, "lj_get_frame_count");
}

int lj_get_frame_count(lua_State *L) {
/* 	Gagent.jerr = (*Gagent.jvmti)->GetFrameCount(Gagent.jvmti, thread, &limit); */
/* 	check_jvmti_error(Gagent.jvmti, Gagent.jerr); */
  lua_pushinteger(L, 17);
  return 1; /* one result */
}
