/*
 * Java utilities for JNI and JVMTI APIs.
 */
#ifndef JNI_UTIL_H_
#define JNI_UTIL_H_

#include <jni.h>
#include <jvmti.h>

typedef struct {
  char *cls;
  char *func;
  char *sig;
  jclass jcls;
  jmethodID mid;
} method_decl;

jvmtiError free_jvmti_refs(jvmtiEnv *jvmti, ...);
jvmtiEventCallbacks *get_jvmti_callbacks();
jvmtiError event_change(jvmtiEnv *jvmti, jvmtiEventMode mode,
			jvmtiEvent type, jthread thread);

/* utility macros to check and clear exceptions on JNI env */
#define EXCEPTION_CLEAR(JNI)			\
  do {						\
    if((*(JNI))->ExceptionCheck(JNI)) {		\
      (*(JNI))->ExceptionClear(JNI);		\
    }						\
  } while(0)

#include "lua_java.h" /* co-dependency for lj_print_message... */

#define EXCEPTION_CHECK(JNI)						\
  do {									\
    if ((*(JNI))->ExceptionCheck(JNI)) {				\
      if(IsDebuggerPresent())						\
	DebugBreak();							\
      lj_print_message("EXCEPTION EXISTS at %s:%d\n", __FILE__, __LINE__); \
      (*(JNI))->ExceptionDescribe(JNI);					\
      (*(JNI))->ExceptionClear(JNI);					\
    }									\
  } while(0)

#define EV_ENABLET(EVTYPE, EVTHR) \
  event_change(lj_jvmti, JVMTI_ENABLE, JVMTI_EVENT_##EVTYPE, (EVTHR))
#define EV_DISABLET(EVTYPE, EVTHR) \
  event_change(lj_jvmti, JVMTI_DISABLE, JVMTI_EVENT_##EVTYPE, (EVTHR))

#endif /* JNI_UTIL_H_ */
