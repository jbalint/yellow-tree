/*
 * JNI stuffings
 */

#include <jni.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else  /* _WIN32 */
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#define IsDebuggerPresent() 1
/* the JVM can delay signal delivery so this doesn't usually work well */
#define DebugBreak() kill(getpid(), SIGCONT)
#endif /* _WIN32 */

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
