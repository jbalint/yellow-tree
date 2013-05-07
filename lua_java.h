#ifndef LUA_JAVA_H_
#define LUA_JAVA_H_

#include <jvmti.h>
#include <jni.h>

#include <lua.h>
#include <lauxlib.h>

#include <stdarg.h>

void lj_init(lua_State *L, JavaVM *jvm, jvmtiEnv *jvmti);
void lj_set_jni(JNIEnv *jni);
void lj_set_jvm_exec_monitor(jrawMonitorID mon);
void lj_print_message(const char *format, ...);

#endif /* LUA_JAVA_H_ */
