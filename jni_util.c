#include "jni_util.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

/*
 * Lookup a method declaration. Instance method is searched first and
 * class method is tried second.
 */
jmethodID method_decl_lookup(jvmtiEnv *jvmti, JNIEnv *jni,
			     const char *class_name, const char *method_name,
			     const char *args, const char *ret)
{
  /* int i; */
  /* jint method_count; */
  /* char *mname; */
  /* char *msig; */

  jvmtiError jerr;
  jclass class;
  jmethodID method_id;
  char *sig;

  /* get class */
  class = (*jni)->FindClass(jni, class_name);
  EXCEPTION_CLEAR(jni);
  if(class == NULL)
    return NULL;

  /* build signature string */
  sig = malloc(strlen(args) + strlen(ret) + 10);
  sprintf(sig, "(%s)%s", args, ret);

  /* try instance method */
  method_id = (*jni)->GetMethodID(jni, class, method_name, sig);
  EXCEPTION_CLEAR(jni);

  /* otherwise try static method */
  if(method_id == NULL)
    method_id = (*jni)->GetStaticMethodID(jni, class, method_name, sig);
  EXCEPTION_CLEAR(jni);
  free(sig);

  return method_id;
}

/*
 * Find the byte code index of a line number in a given method.
 */
jint method_find_line_bytecode_index(jvmtiEnv *jvmti, jmethodID method_id, jint line_num)
{
  jint linescount;
  jint bytecode_index = 0;
  int i;
  jvmtiLineNumberEntry *lines;
  jvmtiError jerr;
  jerr = (*jvmti)->GetLineNumberTable(jvmti, method_id, &linescount, &lines);
  /* TODO don't check jvmti error for native methods, and not debug methods */
  /* TODO check error */
  /* if(check_jvmti_error(jvmti, jerr) != JVMTI_ERROR_NONE) */
  /*   return 0; */
  for(i = 0; i < linescount; ++i)
  {
    if(lines[i].line_number == line_num)
    {
      bytecode_index = lines[i].start_location;
      break;
    }
  }
  (*jvmti)->Deallocate(jvmti, (unsigned char *)lines);
  return bytecode_index;
}
