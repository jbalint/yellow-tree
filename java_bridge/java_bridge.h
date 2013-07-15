#ifndef JAVA_BRIDGE_H_
#define JAVA_BRIDGE_H_

#include <lua.h>

#include <jvmti.h>
#include <jni.h>

/* we need to keep class paired with field_id due to JVMTI API */
typedef struct {
  jfieldID field_id;
  jclass class;
} lj_field_id;

void new_jmonitor(lua_State *L, jrawMonitorID monitor, const char *name);
void new_jfield_id(lua_State *L, jfieldID field_id, jclass class);
void new_jmethod_id(lua_State *L, jmethodID method_id);

void new_string(lua_State *L, JNIEnv *jni, jstring string);

void new_jobject(lua_State *L, jobject object);

#endif /* JAVA_BRIDGE_H_ */
