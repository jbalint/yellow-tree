#include "myjni.h"
#include "jni_util.h"
#include "lua_interface.h"
#include "lua_java.h"
#include "java_bridge.h"
#include "lj_internal.h"

static int lj_set_field_access_watch(lua_State *L)
{
	lj_field_id *field_id;

	field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id");
	lua_pop(L, 1);

	lj_err = (*current_jvmti())->SetFieldAccessWatch(current_jvmti(), field_id->class, field_id->field_id);
	lj_check_jvmti_error(L);

	return 0;
}

static int lj_set_field_modification_watch(lua_State *L)
{
	lj_field_id *field_id;

	field_id = (lj_field_id *)luaL_checkudata(L, 1, "jfield_id");
	lua_pop(L, 1);

	lj_err = (*current_jvmti())->SetFieldModificationWatch(current_jvmti(), field_id->class, field_id->field_id);
	lj_check_jvmti_error(L);

	return 0;
}

void lj_watch_register(lua_State *L)
{
	lua_register(L, "lj_set_field_access_watch", lj_set_field_access_watch);
	/* lua_register(L, "lj_clear_field_access_watch", lj_clear_field_access_watch); */
	lua_register(L, "lj_set_field_modification_watch", lj_set_field_modification_watch);
	/* lua_register(L, "lj_clear_field_modification_watch", lj_clear_field_modification_watch); */
}
