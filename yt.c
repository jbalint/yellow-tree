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
  JavaVM *jvm;
  jvmtiEnv *jvmti; /* global JVMTI reference */
  jvmtiError jerr; /* for convenience, NOT thread safe */
  jlocation ss_target; /* target BCI (of ssmethod) for single stepping */
  jmethodID ss_mid;
  jint ss_destheight; /* our destination stack height */
  int ss_canenter;
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

static void JNICALL command_loop_thread(jvmtiEnv *jvmti, JNIEnv *jni, void *arg)
{
  lua_start_cmd(agent_options);
}

static void JNICALL
cbVMInit(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread)
{
  jclass thread_class;
  jmethodID thread_ctor;
  jthread cmd_thread;
  jstring cmd_thread_name;
  jrawMonitorID thread_resume_monitor;

  /* create raw monitor used for sync with the Lua environment */
  (*Gagent.jvmti)->CreateRawMonitor(Gagent.jvmti, "yellow_tree_thread_resume_monitor", &thread_resume_monitor);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);

  /* initialize Lua side */
  lua_interface_init(Gagent.jvm, Gagent.jvmti, thread_resume_monitor);

  lj_print_message("-------====---------\n");
  lj_print_message("Yellow Tree Debugger\n");
  lj_print_message("-------====---------\n");
  fflush(stdout);

  thread_class = (*jni)->FindClass(jni, "java/lang/Thread");
  assert(thread_class);
  thread_ctor = (*jni)->GetMethodID(jni, thread_class, "<init>", "(Ljava/lang/String;)V");
  assert(thread_ctor);

  /* start and run the debugger command loop */
  cmd_thread_name = (*jni)->NewStringUTF(jni, "Yellow Tree Command Interpreter");
  assert(cmd_thread_name);
  cmd_thread = (*jni)->NewObject(jni, thread_class, thread_ctor, cmd_thread_name);
  assert(cmd_thread);
  (*Gagent.jvmti)->RunAgentThread(Gagent.jvmti, cmd_thread, command_loop_thread,
  								  NULL, JVMTI_THREAD_NORM_PRIORITY);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);

  /* wait for debugger to begin execution */
  (*Gagent.jvmti)->RawMonitorEnter(Gagent.jvmti, thread_resume_monitor);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  (*Gagent.jvmti)->RawMonitorWait(Gagent.jvmti, thread_resume_monitor, 0);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  (*Gagent.jvmti)->RawMonitorExit(Gagent.jvmti, thread_resume_monitor);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
}

void JNICALL
cbVMDeath(jvmtiEnv *jvmti, JNIEnv *jni)
{
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *jvm, char *options, void *reserved)
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
  caps.can_force_early_return = 1;

  rc = (*jvm)->GetEnv(jvm, (void **)&jvmti, JVMTI_VERSION_1_0);
  if(rc < 0)
  {
	fprintf(stderr, "Failed to get JVMTI env\n");
	return JNI_ERR;
  }

  Gagent.jvm = jvm;
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
  /* Check that any calls to SetEventNotificationMode are valid in the
     OnLoad phase before calling here. */
  Gagent.jerr = event_change(Gagent.jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  Gagent.jerr = event_change(Gagent.jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);

  return JNI_OK;
}

JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM *jvm, char *options, void *reserved)
{
  assert(!"ERR: attach not supported");
  return JNI_ERR;
}

JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *jvm)
{
}
