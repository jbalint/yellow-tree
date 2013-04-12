/* -*- tab-width: 4 -*- */
#include <jvmti.h>
#include <jni.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#endif

#include "myjni.h"
#include "jni_util.h"

#include "lua_interface.h"
#include "lua_java.h"

static const char *agent_options;

static struct agent_globals {
  jvmtiEnv *jvmti; /* global JVMTI reference */
  jvmtiError jerr; /* for convenience, NOT thread safe */
  jlocation ss_target; /* target BCI (of ssmethod) for single stepping */
  jmethodID ss_mid;
  jint ss_destheight; /* our destination stack height */
  int ss_canenter;
  /* this is really not a good idea -
	 last JNI env, used when debugger is interrupted by a signal
	 1. last JNI could be from a thread that has since exited
  */
  JNIEnv *sigjni;
  jrawMonitorID exec_monitor;
} Gagent;

static jvmtiError
check_jvmti_error(jvmtiEnv *jvmti, jvmtiError jerr)
{
  char *errmsg;
  if(jerr != JVMTI_ERROR_NONE)
  {
	fprintf(stderr, "Error %d from JVMTI", jerr);
    if((*jvmti)->GetErrorName(jvmti, jerr, &errmsg) == JVMTI_ERROR_NONE)
    {
	  fprintf(stderr, ": %s", errmsg);
      (*jvmti)->Deallocate(jvmti, (unsigned char *)errmsg);
    }
    fprintf(stderr, "\n");
    if(IsDebuggerPresent())
      DebugBreak();
	abort();
  }
  return jerr;
}

#define EV_ENABLE(EVTYPE) \
  (Gagent.jerr = event_change(Gagent.jvmti, JVMTI_ENABLE, \
							  JVMTI_EVENT_##EVTYPE, NULL))
#define EV_ENABLET(EVTYPE, EVTHR) \
  (Gagent.jerr = event_change(Gagent.jvmti, JVMTI_ENABLE, \
							  JVMTI_EVENT_##EVTYPE, (EVTHR)))
#define EV_DISABLE(EVTYPE) \
  (Gagent.jerr = event_change(Gagent.jvmti, JVMTI_DISABLE, \
							  JVMTI_EVENT_##EVTYPE, NULL))
#define EV_DISABLET(EVTYPE, EVTHR) \
  (Gagent.jerr = event_change(Gagent.jvmti, JVMTI_DISABLE, \
							  JVMTI_EVENT_##EVTYPE, (EVTHR)))

#ifndef _WIN32
static void
signal_handler(int sig)
{
  //TODO...?
}

static void
set_signal_handler()
{
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = signal_handler;
  sigemptyset(&act.sa_mask);
  if(sigaction(SIGINT, &act, NULL))
	fprintf(stderr, "Failed to modify signal disposition\n");
}
#endif /* !_WIN32 */

static void JNICALL command_loop_thread(jvmtiEnv *jvmti, JNIEnv *jni, void *arg)
{
  lua_start(jni, agent_options);
}

static void JNICALL
cbVMInit(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread)
{
  jclass thread_class;
  jmethodID thread_ctor;
  jthread agent_thread;
  jstring thread_name;

  /* Enable event notifications (these must be set in live phase) */
  EV_ENABLE(BREAKPOINT);

#ifndef _WIN32
  /* This needs to be fixed to avoid JVMTI_ERROR_UNATTACHED_THREAD
	 when dumping first stack from in Lua command loop */
  set_signal_handler();
#endif

  lua_interface_init(Gagent.jvmti, Gagent.exec_monitor);

  lj_print_message("-------====---------\n");
  lj_print_message("Yellow Tree Debugger\n");
  lj_print_message("-------====---------\n");
  fflush(stdout);

  /* start and run the debugger command loop */
  thread_class = (*jni)->FindClass(jni, "java/lang/Thread");
  assert(thread_class);
  thread_ctor = (*jni)->GetMethodID(jni, thread_class, "<init>", "(Ljava/lang/String;)V");
  assert(thread_ctor);
  thread_name = (*jni)->NewStringUTF(jni, "Yellow Tree Lua Execution");
  assert(thread_name);
  agent_thread = (*jni)->NewObject(jni, thread_class, thread_ctor, thread_name);
  assert(agent_thread);
  (*Gagent.jvmti)->RunAgentThread(Gagent.jvmti, agent_thread, command_loop_thread,
  								  NULL, JVMTI_THREAD_NORM_PRIORITY);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);

  /* wait for debugger to begin execution */
  (*Gagent.jvmti)->RawMonitorEnter(Gagent.jvmti, Gagent.exec_monitor);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  (*Gagent.jvmti)->RawMonitorWait(Gagent.jvmti, Gagent.exec_monitor, 0);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  (*Gagent.jvmti)->RawMonitorExit(Gagent.jvmti, Gagent.exec_monitor);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
}

void JNICALL
cbVMDeath(jvmtiEnv *jvmti, JNIEnv *jni)
{
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
  jvmtiEventCallbacks *evCbs;
  jvmtiCapabilities caps;
  jvmtiEnv *jvmti;
  jint rc;
  jint jvmtiVer;

  agent_options = options ? strdup(options) : "";
  evCbs = get_jvmti_callbacks();

  memset(evCbs, 0, sizeof(*evCbs));
  memset(&caps, 0, sizeof(caps));

  evCbs->VMInit = cbVMInit;
  evCbs->VMDeath = cbVMDeath;
  caps.can_generate_breakpoint_events = 1;
  caps.can_generate_method_entry_events = 1;
  caps.can_generate_method_exit_events = 1;
/*   caps.can_tag_objects = 1; */
  caps.can_get_source_file_name = 1;
  caps.can_get_line_numbers = 1;
  caps.can_access_local_variables = 1;
  caps.can_generate_single_step_events = 1; /* Used for line-oriented stepping */
/*   caps.can_generate_frame_pop_events = 1; */

  rc = (*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1_0);
  if(rc < 0)
  {
	fprintf(stderr, "Failed to get JVMTI env\n");
	return JNI_ERR;
  }

  Gagent.jvmti = jvmti;
  Gagent.jerr = (*Gagent.jvmti)->GetVersionNumber(Gagent.jvmti, &jvmtiVer);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  printf("JVMTI version %d.%d.%d\n",
		 (jvmtiVer & JVMTI_VERSION_MASK_MAJOR) >> JVMTI_VERSION_SHIFT_MAJOR,
		 (jvmtiVer & JVMTI_VERSION_MASK_MINOR) >> JVMTI_VERSION_SHIFT_MINOR,
		 (jvmtiVer & JVMTI_VERSION_MASK_MICRO) >> JVMTI_VERSION_SHIFT_MICRO);
  Gagent.jerr = (*Gagent.jvmti)->AddCapabilities(Gagent.jvmti, &caps);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  Gagent.jerr = (*Gagent.jvmti)->SetEventCallbacks(Gagent.jvmti,
												   evCbs, sizeof(jvmtiEventCallbacks));
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  EV_ENABLE(VM_INIT);
  EV_ENABLE(VM_DEATH);
/*   EV_ENABLE(THREAD_START); */
/*   EV_ENABLE(THREAD_END); */
  /* Check that any calls to SetEventNotificationMode are valid in the
     OnLoad phase before calling here. */

  /* create raw monitor used for sync with the Lua environment */
  (*Gagent.jvmti)->CreateRawMonitor(Gagent.jvmti, "yellow_tree_lua", &Gagent.exec_monitor);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);

  return JNI_OK;
}

JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM *vm, char *options, void *reserved)
{
  assert(!"ERR: attach not supported");
  return JNI_ERR;
}

JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm)
{
}
