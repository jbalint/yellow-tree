#ifndef LJ_INTERNAL_H_
#define LJ_INTERNAL_H_

#include "lua_java.h"

jobject get_current_java_thread();
jvmtiEnv *current_jvmti();
JNIEnv *current_jni();

void lj_check_jvmti_error_internal(lua_State *, const char *, int, const char *);
#define lj_check_jvmti_error(L) lj_check_jvmti_error_internal(L, __FILE__, __LINE__, __FUNCTION__)

extern jvmtiError lj_err;

#define EV_ENABLET(EVTYPE, EVTHR) \
  event_change(current_jvmti(), JVMTI_ENABLE, JVMTI_EVENT_##EVTYPE, (EVTHR))
#define EV_DISABLET(EVTYPE, EVTHR) \
  event_change(current_jvmti(), JVMTI_DISABLE, JVMTI_EVENT_##EVTYPE, (EVTHR))

#endif /* LJ_INTERNAL_H_ */
