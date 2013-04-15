#include "jni_util.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Shared instance between all JVMTI users -
   because the JVMTI API doesn't have GetEventCallbacks(),
   we need to allow adding/changing callbacks without knowing about
   other code. */
static jvmtiEventCallbacks jvmti_callbacks;

/*
 * Free a set of JVMTI references. Pass (void *)-1 as the last parameter.
 * Be *sure* to cast the last argument to a pointer.
 */
jvmtiError free_jvmti_refs(jvmtiEnv *jvmti, ...)
{
  jvmtiError jerr = JVMTI_ERROR_NONE;
  va_list s;
  unsigned char *p;
  va_start(s, jvmti);
  while((p = va_arg(s, unsigned char *)) != (unsigned char *)-1)
  {
    if(!p)
      continue;
    jerr = (*jvmti)->Deallocate(jvmti, p);
    /* if(check_jvmti_error(Gagent.jvmti, Gagent.jerr) != JVMTI_ERROR_NONE) */
    /*   jerr = Gagent.jerr; */
  }
  va_end(s);
  return jerr;
}

jvmtiEventCallbacks *get_jvmti_callbacks()
{
  return &jvmti_callbacks;
}

jvmtiError
event_change(jvmtiEnv *jvmti, jvmtiEventMode mode,
	     jvmtiEvent type, jthread thread)
{
  jvmtiError jerr = (*jvmti)->SetEventNotificationMode(jvmti, mode, type, thread);
  /* if(jerr == JVMTI_ERROR_NONE) */
  /*   event_states[type - JVMTI_MIN_EVENT_TYPE_VAL] = mode; */
  return jerr;
}
