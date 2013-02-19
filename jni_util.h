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
jint method_find_line_bci(jvmtiEnv *jvmti, method_decl *method, jint linenum);

/* utility macros to check and clear exceptions on JNI env */
#define EXCEPTION_CLEAR(JNI)				\
  do {										\
    if((*(JNI))->ExceptionCheck(JNI)) {		\
      (*(JNI))->ExceptionClear(JNI);		\
    }										\
  } while(0)

#define EXCEPTION_CHECK(JNI)					\
  do {											\
    if ((*(JNI))->ExceptionCheck(JNI)) {		\
      if(IsDebuggerPresent())											\
		DebugBreak();													\
      fprintf(stderr, "EXCEPTION EXISTS at %s:%d\n", __FILE__, __LINE__); \
      (*(JNI))->ExceptionDescribe(JNI);									\
      (*(JNI))->ExceptionClear(JNI);									\
    }																	\
  } while(0)

#endif /* JNI_UTIL_H_ */
