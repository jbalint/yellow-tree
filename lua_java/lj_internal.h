#include "lua_java.h"

jobject get_current_java_thread();
jvmtiEnv *current_jvmti();

extern jvmtiError lj_err;

#define EV_ENABLET(EVTYPE, EVTHR) \
  event_change(current_jvmti(), JVMTI_ENABLE, JVMTI_EVENT_##EVTYPE, (EVTHR))
#define EV_DISABLET(EVTYPE, EVTHR) \
  event_change(current_jvmti(), JVMTI_DISABLE, JVMTI_EVENT_##EVTYPE, (EVTHR))
