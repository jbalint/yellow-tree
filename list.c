#include "list.h"
#include <string.h>
#include <assert.h>

/* list_add: Add an element to the list and return the new list
 * head. This allows the initial list pointer to be NULL and a
 * new head will be created.
 */
list *list_add(list *l, void *d)
{
  list *orig = l;
  list *lnew = LMALLOC(sizeof(list));
  assert(lnew);
  memset(lnew, 0, sizeof(list));
  lnew->d = d;
  for(; l && l->n; l = l->n);
  if(l)
  {
#ifdef LIST_HAS_ID
    lnew->id = l->id + 1;
#endif
    l->n = lnew;
    return orig;
  }
  else
  {
    l = lnew;
    return l;
  }
}

/* list_next: Grab the next element of the list and it's
 * data element. If there's no next element, the data pointer
 * will not be altered. Returns the next list node.
 */
list *list_next(list *l, void **d)
{
  if(l && l->n)
  {
    l = l->n;
    if(d)
      *d = l->d;
    return l;
  }
  else
    return NULL;
}

/* list_delete_elem: Delete an element in the list matching a given data
 * pointer. There is no indication whether or not an element is actually
 * removed from the list. Returns the new list head.
 */
list *list_delete_elem(list *l, void *d)
{
  list *prev = NULL;
  list *orig = l;
  for(; l && l->d != d && l->n; prev = l, l = l->n);
  if(l && l->d == d)
  {
    if(orig == l)
      orig = l->n;
    else if(prev)
      prev->n = l->n;
    LFREE(l);
  }
  return orig;
}

#ifdef LIST_HAS_ID
/* list_get_at: Find the element of the list with the specified id. If a
 * pointer d is given, the data will be return if list node is found. Returns
 * the list node if found, NULL otherwise.
 */
list *list_get_at(list *l, int id, void **d)
{
  for(; l && l->id != id && l->n; l = l->n);
  if(l->id == id)
  {
    if(d)
      *d = l->d;
    return l;
  }
  else
    return NULL;
}
#endif /* LIST_HAS_ID */

/*
 * Find the last element of the given list.
 */
list *list_end(list *l)
{
  for(; l && l->n; l = l->n);
  return l;
}

/*
 * Get the data element for the head of the given list. This can be used
 * as an iterator:
 * for(l = list_start(some_list, &some_data); l; l = list_next(l, &some_list))
 */
list *list_start(list *l, void **d)
{
  if(l && d)
    *d = l->d;
  return l;
}
