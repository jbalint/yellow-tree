/*
 * Linked list
 *
 * Optional cpp macros:
 * * LMALLOC - you can #define your own malloc(), such as g_malloc
 * * LFREE - same for free()
 * * LIST_HAS_ID - #define this to add an id counter on each list element
 */
#ifndef LIST_H_
#define LIST_H_

#include <stdlib.h>

typedef struct list {
  struct list *n;
  void *d;
#ifdef LIST_HAS_ID
  int id; /* autoincrement id counter, for convenience */
#endif
} list;

#ifndef LMALLOC
#define LMALLOC malloc
#endif
#ifndef LFREE
#define LFREE free
#endif

list *list_add(list *l, void *d);
list *list_next(list *l, void **d);
list *list_delete_elem(list *l, void *d);
#ifdef LIST_HAS_ID
list *list_get_at(list *l, int id, void **d);
#endif
list *list_end(list *l);
list *list_start(list *l, void **d);

#endif /* LIST_H_ */
