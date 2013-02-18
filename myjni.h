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
