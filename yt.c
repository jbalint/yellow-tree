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

static struct agent_globals {
  jvmtiEnv *jvmti; /* global JVMTI reference */
  jvmtiError jerr; /* for convenience, NOT thread safe */
  jint depth;
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

static char
event_states[JVMTI_MAX_EVENT_TYPE_VAL - JVMTI_MIN_EVENT_TYPE_VAL];

static jvmtiError
event_change(jvmtiEnv *jvmti, jvmtiEventMode mode,
			 jvmtiEvent type, jthread thread)
{
  jvmtiError jerr = (*jvmti)->SetEventNotificationMode(jvmti, mode, type, thread);
  check_jvmti_error(jvmti, jerr);
  if(jerr == JVMTI_ERROR_NONE)
	event_states[type - JVMTI_MIN_EVENT_TYPE_VAL] = mode;
  return jerr;
}

/*static*/ jvmtiEventMode 
event_status(jvmtiEvent type)
{
  return event_states[type - JVMTI_MIN_EVENT_TYPE_VAL];
}

static void
dump_locals(JNIEnv *jni)
{
  jthread thread = NULL;
  jlocation bci = 0;
  jmethodID curmid;
  jvmtiLocalVariableEntry *lvtable;
  jint lvcount;
  jint i;

  Gagent.jerr = (*Gagent.jvmti)->GetFrameLocation(Gagent.jvmti, thread,
												  Gagent.depth, &curmid, &bci);
  if(check_jvmti_error(Gagent.jvmti, Gagent.jerr) != JVMTI_ERROR_NONE)
	return;
  Gagent.jerr = (*Gagent.jvmti)->GetLocalVariableTable(Gagent.jvmti, curmid,
													   &lvcount, &lvtable);
  if(check_jvmti_error(Gagent.jvmti, Gagent.jerr) != JVMTI_ERROR_NONE)
	return;
  for(i = 0; i < lvcount; ++i)
  {
	jvmtiLocalVariableEntry *lv = &lvtable[i];
	if(bci >= lv->start_location && bci <= lv->start_location + lv->length)
	{
	  jint ji;
	  jlong j;
	  jfloat f;
	  jdouble d;
	  jobject o;
	  printf("[%d] %s(%s)", i, lv->name, lv->signature);
	  switch(lv->signature[0])
	  {
	  case '[': /* array */
		printf("<array>");
		break;
	  case 'L': /* object */
		Gagent.jerr = (*Gagent.jvmti)->GetLocalObject(Gagent.jvmti, thread,
													  Gagent.depth, lv->slot, &o);
		check_jvmti_error(Gagent.jvmti, Gagent.jerr);
		Gagent.jerr = (*Gagent.jvmti)->GetObjectHashCode(Gagent.jvmti, o, &ji);
		check_jvmti_error(Gagent.jvmti, Gagent.jerr);
		{
		  jclass clsobject = (*jni)->FindClass(jni, "java/lang/Object");
		  jmethodID midtostring = (*jni)->GetMethodID(jni, clsobject,
													  "toString", "()Ljava/lang/String;");
		  jstring jobjstr = (*jni)->CallObjectMethod(jni, o, midtostring);
		  const char *objstr = (*jni)->GetStringUTFChars(jni, jobjstr, NULL);
		  printf("@%x=%s", ji, objstr);
		  (*jni)->ReleaseStringUTFChars(jni, jobjstr, objstr);
		}
		break;
	  case 'I':
		Gagent.jerr = (*Gagent.jvmti)->GetLocalInt(Gagent.jvmti, thread,
												   Gagent.depth, lv->slot, &ji);
		check_jvmti_error(Gagent.jvmti, Gagent.jerr);
		printf("=%d", ji);
		break;
	  case 'J':
		Gagent.jerr = (*Gagent.jvmti)->GetLocalLong(Gagent.jvmti, thread,
													Gagent.depth, lv->slot, &j);
		check_jvmti_error(Gagent.jvmti, Gagent.jerr);
		printf("=%ld", j);
		break;
	  case 'F':
		Gagent.jerr = (*Gagent.jvmti)->GetLocalFloat(Gagent.jvmti, thread,
													 Gagent.depth, lv->slot, &f);
		check_jvmti_error(Gagent.jvmti, Gagent.jerr);
		printf("=%g", f);
		break;
	  case 'D':
		Gagent.jerr = (*Gagent.jvmti)->GetLocalDouble(Gagent.jvmti, thread,
													  Gagent.depth, lv->slot, &d);
		check_jvmti_error(Gagent.jvmti, Gagent.jerr);
		printf("=%g", f);
		break;
	  }
	  printf("\n");
	}
	free_jvmti_refs(Gagent.jvmti, lv->name, lv->signature, lv->generic_signature, (void *)-1);
  }
  free_jvmti_refs(Gagent.jvmti, lvtable, (void *)-1);
}

static void
step_next_line(JNIEnv *env, int intomethod)
{
  /* TODO clean this up to only display the appropriate message(s) */
  jint lines = 0;
  jvmtiLineNumberEntry *linetable = NULL;
  jmethodID curmid;
  jthread thread = NULL;
  jlocation bci;
  jint i;
  jint frames;
  Gagent.jerr = (*Gagent.jvmti)->GetCurrentThread(Gagent.jvmti, &thread);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  Gagent.jerr = (*Gagent.jvmti)->GetFrameLocation(Gagent.jvmti, thread,
												  0 /* TODO any reason not to be zero? */,
												  &curmid, &bci);
  /* this will happen if 'step' at the very beginning of the program */
  if(Gagent.jerr == JVMTI_ERROR_NO_MORE_FRAMES)
  {
	printf("Stepping with frame location unknown\n");
  }
  else
  {
	check_jvmti_error(Gagent.jvmti, Gagent.jerr);
	Gagent.jerr = (*Gagent.jvmti)->GetLineNumberTable(Gagent.jvmti, curmid, &lines, &linetable);
	/* TODO test how this actually works */
	if(Gagent.jerr == JVMTI_ERROR_NATIVE_METHOD)
	  printf("Stepping through native method\n");
	else
	  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  }
  Gagent.ss_mid = NULL;
  Gagent.ss_target = 0;
  Gagent.ss_destheight = 0;
  Gagent.ss_canenter = intomethod;
  for(i = 0; i < lines; ++i)
  {
	if(linetable[i].start_location > bci)
	{
	/*   printf("Stepping to line %d\n", linetable[i].line_number); */
	  EV_ENABLET(SINGLE_STEP, thread);
	  EV_ENABLET(METHOD_ENTRY, thread);
	  Gagent.ss_mid = curmid;
	  Gagent.ss_target = linetable[i].start_location;
	  goto end;
	}
  }
  Gagent.jerr = (*Gagent.jvmti)->GetFrameCount(Gagent.jvmti, thread, &frames);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  Gagent.ss_destheight = frames - 1;
  EV_ENABLET(METHOD_EXIT, thread);
end:
  free_jvmti_refs(Gagent.jvmti, linetable, (void *)-1);
}

static void
frame_adjust(int fradj, int absolute)
{
  jthread thread = NULL;
  jint frames;
/*   Gagent.jerr = (*Gagent.jvmti)->GetCurrentThread(Gagent.jvmti, &thread); */
/*   if(check_jvmti_error(Gagent.jvmti, Gagent.jerr) != JVMTI_ERROR_NONE) */
/* 	return; */
  Gagent.jerr = (*Gagent.jvmti)->GetFrameCount(Gagent.jvmti, thread, &frames);
  if(check_jvmti_error(Gagent.jvmti, Gagent.jerr) != JVMTI_ERROR_NONE)
	return;
  if((!absolute && (Gagent.depth + fradj >= frames ||
					Gagent.depth + fradj < 0)) ||
	 (absolute && (fradj >= frames || fradj < 0)))
  {
	printf("Invalid frame\n");
	return;
  }
  if(absolute)
	Gagent.depth = fradj;
  else
	Gagent.depth += fradj;
  /* dump_stack(1, Gagent.depth); */
}

static void JNICALL
cbFramePop(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID mid,
		   jboolean excpopped)
{
  assert(!"Not used");
  EV_DISABLE(FRAME_POP);
  lua_command_loop(jni);
}

static void JNICALL
cbSingleStep(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread,
			 jmethodID mid, jlocation location)
{
  if((Gagent.ss_mid && mid != Gagent.ss_mid) ||
	 location < Gagent.ss_target)
	return;
  EV_DISABLET(SINGLE_STEP, thread);
  EV_DISABLET(METHOD_ENTRY, thread);
  lua_command_loop(jni);
}

static void JNICALL
cbMethodEntry(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID mid)
{
  jclass declcls;
  char *clssig;
  /**/
  EV_DISABLET(SINGLE_STEP, thread);
  EV_DISABLET(METHOD_ENTRY, thread);
  if(Gagent.ss_canenter)
  {
	Gagent.jerr = (*Gagent.jvmti)->GetMethodDeclaringClass(Gagent.jvmti, mid, &declcls);
	check_jvmti_error(Gagent.jvmti, Gagent.jerr);
	Gagent.jerr = (*Gagent.jvmti)->GetClassSignature(Gagent.jvmti, declcls, &clssig, NULL);
	check_jvmti_error(Gagent.jvmti, Gagent.jerr);
	if(!strncmp(clssig, "Lsun/", 5) ||
	   !strncmp(clssig, "Ljava/", 6))
	{
	  free_jvmti_refs(Gagent.jvmti, clssig, (void *)-1);
	  EV_ENABLET(SINGLE_STEP, thread);
	  EV_ENABLET(METHOD_ENTRY, thread);
	  return;
	}
	free_jvmti_refs(Gagent.jvmti, clssig, (void *)-1);
	/**/
	lua_command_loop(jni);
  }
  else
  {
	EV_ENABLET(METHOD_EXIT, thread);
	Gagent.jerr = (*Gagent.jvmti)->GetFrameCount(Gagent.jvmti, thread,
												 &Gagent.ss_destheight);
	check_jvmti_error(Gagent.jvmti, Gagent.jerr);
	Gagent.ss_destheight--;
  }
}

static void JNICALL
cbMethodExit(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID mid,
			 jboolean expopped, jvalue retval)
{
  jint frames;
  Gagent.jerr = (*Gagent.jvmti)->GetFrameCount(Gagent.jvmti, thread, &frames);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  /* wait til we've popped enough frames */
  if(frames - 1 > Gagent.ss_destheight)
	return;
  EV_DISABLET(METHOD_EXIT, thread);
  EV_ENABLET(SINGLE_STEP, thread);
  EV_ENABLET(METHOD_ENTRY, thread);
}

static void JNICALL
cbBreakpoint(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread,
			 jmethodID mid, jlocation location)
{
  printf("Breakpoint hit\n");
  lua_command_loop(jni);
}

#ifndef _WIN32
static void
signal_handler(int sig)
{
  lua_command_loop(Gagent.sigjni);
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
  lua_command_loop(jni);
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
	 when dumping first stack from in lua_command_loop() */
  set_signal_handler();
#endif
  printf("-------====---------\n");
  printf("Yellow Tree Debugger\n");
  printf("-------====---------\n");

  /* start and run the debugger command loop */
  thread_class = (*jni)->FindClass(jni, "java/lang/Thread");
  assert(thread_class);
  thread_ctor = (*jni)->GetMethodID(jni, thread_class, "<init>", "(Ljava/lang/String;)V");
  assert(thread_ctor);
  thread_name = (*jni)->NewStringUTF(jni, "Yellow Tree Lua Command Loop");
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
  jvmtiEventCallbacks evCbs;
  jvmtiCapabilities caps;
  jvmtiEnv *jvmti;
  jint rc;
  jint jvmtiVer;

  memset(&evCbs, 0, sizeof(evCbs));
  memset(&caps, 0, sizeof(caps));

  evCbs.VMInit = cbVMInit;
  evCbs.VMDeath = cbVMDeath;
  evCbs.Breakpoint = cbBreakpoint;
  evCbs.MethodEntry = cbMethodEntry;
  evCbs.MethodExit = cbMethodExit;
/*   evCbs.ThreadStart = cbThreadStart; */
/*   evCbs.ThreadEnd = cbThreadEnd; */
  evCbs.SingleStep = cbSingleStep;
  evCbs.FramePop = cbFramePop;
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
												   &evCbs, sizeof(evCbs));
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

  lua_interface_init(Gagent.jvmti, Gagent.exec_monitor);

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
