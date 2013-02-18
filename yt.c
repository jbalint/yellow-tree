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

/*
 * Dump the stack of the current thread.
 */
static void
dump_stack(jint limit, jint depth)
{
  jint framecount = 0;
  jvmtiFrameInfo *frames;
  int i;
  int j;
  char *mname;
  char *sig;
  jclass cls;
  char *clsname;
  char *sourcefile = "<unknown_file>";
  int freesf = 0;
  jvmtiLineNumberEntry *lines;
  int linescount = 0;
  int line_number = -1;
  jthread thread = NULL;
/*   Gagent.jerr = (*Gagent.jvmti)->GetCurrentThread(Gagent.jvmti, &thread); */
/*   check_jvmti_error(Gagent.jvmti, Gagent.jerr); */
/*   if(!thread) */
/* 	return; */
  if(limit == -1)
  {
	Gagent.jerr = (*Gagent.jvmti)->GetFrameCount(Gagent.jvmti, thread, &limit);
	check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  }
  frames = malloc(sizeof(jvmtiFrameInfo) * limit);
  Gagent.jerr = (*Gagent.jvmti)->GetStackTrace(Gagent.jvmti, thread, depth,
											   limit, frames, &framecount);
  check_jvmti_error(Gagent.jvmti, Gagent.jerr);
  for(i = 0; i < framecount && i < limit; ++i)
  {
    Gagent.jerr = (*Gagent.jvmti)->GetMethodDeclaringClass(Gagent.jvmti,
														   frames[i].method, &cls);
    check_jvmti_error(Gagent.jvmti, Gagent.jerr);
	if(!cls)
	  continue;
    Gagent.jerr = (*Gagent.jvmti)->GetClassSignature(Gagent.jvmti, cls, &clsname, NULL);
    check_jvmti_error(Gagent.jvmti, Gagent.jerr);
    Gagent.jerr = (*Gagent.jvmti)->GetMethodName(Gagent.jvmti, frames[i].method,
												 &mname, &sig, NULL);
    check_jvmti_error(Gagent.jvmti, Gagent.jerr);
    Gagent.jerr = (*Gagent.jvmti)->GetSourceFileName(Gagent.jvmti, cls, &sourcefile);
    if(Gagent.jerr == JVMTI_ERROR_ABSENT_INFORMATION)
      freesf = 0;
    else
      check_jvmti_error(Gagent.jvmti, Gagent.jerr);
    Gagent.jerr = (*Gagent.jvmti)->GetLineNumberTable(Gagent.jvmti, frames[i].method,
					    &linescount, &lines);
    if(Gagent.jerr == JVMTI_ERROR_NATIVE_METHOD)
    {
      sourcefile = "<native_method>";
      assert(!freesf);
    }
    else if(Gagent.jerr != JVMTI_ERROR_ABSENT_INFORMATION)
      check_jvmti_error(Gagent.jvmti, Gagent.jerr);
    /* link the bytecode offset to LineNumberTable if exists */
    for(j = 0; j < linescount; ++j)
    {
      if(j < (linescount - 1) &&
		 frames[i].location < lines[j+1].start_location)
		break;
      else if(j == (linescount - 1))
		break;
    }
    if(linescount)
      line_number = lines[j].line_number;
    printf("  [%d] %s.%s%s - %ld (%s:%d)\n", depth + i,
		   clsname + 1, mname, sig, (long)frames[i].location,
		   sourcefile, line_number);
    free_jvmti_refs(Gagent.jvmti, mname, sig, clsname, (void *)-1);
    if(freesf)
      (*Gagent.jvmti)->Deallocate(Gagent.jvmti, (unsigned char *)sourcefile);
    if(linescount)
      (*Gagent.jvmti)->Deallocate(Gagent.jvmti, (unsigned char *)lines);
  }
  free(frames);
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

/*   Gagent.jerr = (*Gagent.jvmti)->GetCurrentThread(Gagent.jvmti, &thread); */
/*   if(check_jvmti_error(Gagent.jvmti, Gagent.jerr) != JVMTI_ERROR_NONE) */
/* 	return; */
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
/*   if(frames == 1) */
/*   { */
/* 	printf("Can't do it\n"); */
/* 	goto end; */
/*   } */
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
  dump_stack(1, Gagent.depth);
}

static void
command_loop(JNIEnv *jni)
{
  char cmd[255];
  size_t len;

  Gagent.sigjni = jni; /* TODO ugly */
  dump_stack(1, 0);

  printf("yt> ");
  while(fgets(cmd, 255, stdin))
  {
	len = strlen(cmd) - 1;
	cmd[len] = 0;
	if(!strcmp(cmd, "run") || !strcmp(cmd, "cont"))
	  break;
	else if(!strcmp(cmd, "threads"))
	{
	}
	else if(!strcmp(cmd, "where"))
	  dump_stack(-1, 0);
	else if(!strcmp(cmd, "locals"))
	  dump_locals(jni);
	else if(!strcmp(cmd, "next"))
	{
	  step_next_line(jni, 0);
	  break;
	}
	else if(!strcmp(cmd, "step"))
	{
	  step_next_line(jni, 1);
	  break;
	}
	else if(!strcmp(cmd, "up"))
	  frame_adjust(1, 0);
	else if(!strcmp(cmd, "down"))
	  frame_adjust(-1, 0);
	else if(!strcmp(cmd, "frame"))
	  dump_stack(1, Gagent.depth);
	else if(!strncmp(cmd, "up ", 3))
	  frame_adjust(atoi(cmd + 3), 0);
	else if(!strncmp(cmd, "down ", 5))
	  frame_adjust(-1*atoi(cmd + 5), 0);
	else if(!strncmp(cmd, "frame ", 6))
	  frame_adjust(atoi(cmd + 6), 1);
	printf("yt> ");
  }
  /* reset stack depth... */
  Gagent.depth = 0;
}

static void JNICALL
cbFramePop(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread, jmethodID mid,
		   jboolean excpopped)
{
  assert(!"Not used");
  EV_DISABLE(FRAME_POP);
  command_loop(jni);
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
  command_loop(jni);
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
	command_loop(jni);
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
  command_loop(Gagent.sigjni);
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

static void JNICALL
cbVMInit(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread)
{
  /* Enable event notifications (these must be set in live phase) */
  EV_ENABLE(BREAKPOINT);
#ifndef _WIN32
  /* This needs to be fixed to avoid JVMTI_ERROR_UNATTACHED_THREAD
	 when dumping first stack from in command_loop() */
  set_signal_handler();
#endif
  printf("-------====---------\n");
  printf("Yellow Tree Debugger\n");
  printf("-------====---------\n");
  lua_command_loop(jni);
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

  lua_interface_init();

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
