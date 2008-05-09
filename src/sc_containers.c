/*
  This file is part of the SC Library.
  The SC Library provides support for parallel scientific applications.

  Copyright (C) 2007,2008 Carsten Burstedde, Lucas Wilcox.

  The SC Library is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  The SC Library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the SC Library.  If not, see <http://www.gnu.org/licenses/>.
*/

/* sc.h comes first in every compilation unit */
#include <sc.h>
#include <sc_containers.h>

/* array routines */

sc_array_t         *
sc_array_new (size_t elem_size)
{
  sc_array_t         *array;

  array = SC_ALLOC_ZERO (sc_array_t, 1);

  sc_array_init (array, elem_size);

  return array;
}

void
sc_array_destroy (sc_array_t * array)
{
  SC_FREE (array->array);
  SC_FREE (array);
}

void
sc_array_init (sc_array_t * array, size_t elem_size)
{
  SC_ASSERT (elem_size > 0);

  array->elem_size = elem_size;
  array->elem_count = 0;
  array->byte_alloc = 0;
  array->array = NULL;
}

void
sc_array_reset (sc_array_t * array)
{
  SC_FREE (array->array);
  array->array = NULL;

  array->elem_count = 0;
  array->byte_alloc = 0;
}

#ifdef SC_RESIZE_REALLOC

void
sc_array_resize (sc_array_t * array, size_t new_count)
{
  char               *ptr;
  size_t              newoffs, roundup, newsize;
#ifdef SC_DEBUG
  size_t              oldoffs;
  size_t              i, minoffs;
#endif

#ifdef SC_DEBUG
  oldoffs = array->elem_count * array->elem_size;
#endif
  array->elem_count = new_count;
  newoffs = array->elem_count * array->elem_size;
  roundup = SC_ROUNDUP2_32 (newoffs);
  SC_ASSERT (roundup >= newoffs && roundup <= 2 * newoffs);

  if (newoffs > array->byte_alloc || roundup < array->byte_alloc) {
    array->byte_alloc = roundup;
  }
  else {
#ifdef SC_DEBUG
    if (newoffs < oldoffs) {
      memset (array->array + newoffs, -1, oldoffs - newoffs);
    }
    for (i = oldoffs; i < newoffs; ++i) {
      SC_ASSERT (array->array[i] == (char) -1);
    }
#endif
    return;
  }
  SC_ASSERT (array->byte_alloc >= newoffs);

  newsize = array->byte_alloc;
  ptr = SC_REALLOC (array->array, char, newsize);

  array->array = ptr;
#ifdef SC_DEBUG
  minoffs = SC_MIN (oldoffs, newoffs);
  SC_ASSERT (minoffs <= newsize);
  memset (array->array + minoffs, -1, newsize - minoffs);
#endif
}

#else /* !SC_RESIZE_REALLOC */

void
sc_array_resize (sc_array_t * array, size_t new_count)
{
  char               *ptr;
  size_t              oldoffs, newoffs;
  size_t              roundup, newsize;
#ifdef SC_DEBUG
  size_t              i;
#endif

  if (new_count == 0) {
    sc_array_reset (array);
    return;
  }

  oldoffs = array->elem_count * array->elem_size;
  array->elem_count = new_count;
  newoffs = array->elem_count * array->elem_size;
  roundup = (size_t) SC_ROUNDUP2_64 (newoffs);
  SC_ASSERT (roundup >= newoffs && roundup <= 2 * newoffs);

  if (newoffs > array->byte_alloc) {
    array->byte_alloc = roundup;
  }
  else {
#ifdef SC_DEBUG
    if (newoffs < oldoffs) {
      memset (array->array + newoffs, -1, oldoffs - newoffs);
    }
    for (i = oldoffs; i < newoffs; ++i) {
      SC_ASSERT (array->array[i] == (char) -1);
    }
#endif
    return;
  }
  SC_ASSERT (array->byte_alloc >= newoffs);
  SC_ASSERT (newoffs > oldoffs);

  newsize = array->byte_alloc;
  ptr = SC_ALLOC (char, newsize);
  memcpy (ptr, array->array, oldoffs);
  SC_FREE (array->array);
  array->array = ptr;

#ifdef SC_DEBUG
  memset (array->array + oldoffs, -1, newsize - oldoffs);
#endif
}

#endif /* !SC_RESIZE_REALLOC */

void
sc_array_sort (sc_array_t * array, int (*compar) (const void *, const void *))
{
  qsort (array->array, array->elem_count, array->elem_size, compar);
}

void
sc_array_uniq (sc_array_t * array, int (*compar) (const void *, const void *))
{
  size_t              incount, dupcount;
  size_t              i, j;
  void               *elem1, *elem2, *temp;

  incount = array->elem_count;
  if (incount == 0) {
    return;
  }

  dupcount = 0;                 /* count duplicates */
  i = 0;                        /* read counter */
  j = 0;                        /* write counter */
  elem1 = sc_array_index (array, 0);
  while (i < incount) {
    elem2 = ((i < incount - 1) ? sc_array_index (array, i + 1) : NULL);
    if (i < incount - 1 && compar (elem1, elem2) == 0) {
      ++dupcount;
      ++i;
    }
    else {
      if (i > j) {
        temp = sc_array_index (array, j);
        memcpy (temp, elem1, array->elem_size);
      }
      ++i;
      ++j;
    }
    elem1 = elem2;
  }
  SC_ASSERT (i == incount);
  SC_ASSERT (j + dupcount == incount);
  sc_array_resize (array, j);
}

ssize_t
sc_array_bsearch (sc_array_t * array, const void *key,
                  int (*compar) (const void *, const void *))
{
  ssize_t             index = -1;
  char               *retval;

  retval = (char *)
    bsearch (key, array->array, array->elem_count, array->elem_size, compar);

  if (retval != NULL) {
    index = (ssize_t) ((retval - array->array) / array->elem_size);
    SC_ASSERT (index >= 0 && (size_t) index < array->elem_count);
  }

  return index;
}

unsigned
sc_array_checksum (sc_array_t * array, size_t first_elem)
{
  size_t              first_byte;
  uInt                bytes;
  uLong               crc;

  SC_ASSERT (first_elem <= array->elem_count);

  crc = adler32 (0L, Z_NULL, 0);
  if (array->elem_count == 0) {
    return (unsigned) crc;
  }

  first_byte = first_elem * array->elem_size;
  bytes = (uInt) ((array->elem_count - first_elem) * array->elem_size);
  crc = adler32 (crc, (const Bytef *) (array->array + first_byte), bytes);

  return (unsigned) crc;
}

size_t
sc_array_pqueue_add (sc_array_t * array, void *temp,
                     int (*compar) (const void *, const void *))
{
  int                 comp;
  size_t              parent, child, swaps;
  const size_t        size = array->elem_size;
  void               *p, *c;

  /* this works on a pre-allocated array */
  SC_ASSERT (array->elem_count > 0);

  swaps = 0;
  child = array->elem_count - 1;
  c = array->array + (size * child);
  while (child > 0) {
    parent = (child - 1) / 2;
    p = array->array + (size * parent);

    /* compare child to parent */
    comp = compar (p, c);
    if (comp <= 0) {
      break;
    }

    /* swap child and parent */
    memcpy (temp, c, size);
    memcpy (c, p, size);
    memcpy (p, temp, size);
    ++swaps;

    /* walk up the tree */
    child = parent;
    c = p;
  }

  return swaps;
}

size_t
sc_array_pqueue_pop (sc_array_t * array, void *result,
                     int (*compar) (const void *, const void *))
{
  int                 comp;
  size_t              new_count, swaps;
  size_t              parent, child, child1;
  const size_t        size = array->elem_size;
  void               *p, *c, *c1;
  void               *temp;

  /* array must not be empty */
  SC_ASSERT (array->elem_count > 0);

  swaps = 0;
  new_count = array->elem_count - 1;

  /* extract root */
  parent = 0;
  p = array->array + (size * parent);
  memcpy (result, p, size);

  /* copy the last element to the top and reuse it as temp storage */
  temp = array->array + (size * new_count);
  if (new_count > 0) {
    memcpy (p, temp, size);
  }

  /* sift down the tree */
  while ((child = 2 * parent + 1) < new_count) {
    c = array->array + (size * child);

    /* check if child has a sibling and use that one if it is smaller */
    if ((child1 = 2 * parent + 2) < new_count) {
      c1 = array->array + (size * child1);
      comp = compar (c, c1);
      if (comp > 0) {
        child = child1;
        c = c1;
      }
    }

    /* sift down the parent if it is larger */
    comp = compar (p, c);
    if (comp <= 0) {
      break;
    }

    /* swap child and parent */
    memcpy (temp, c, size);
    memcpy (c, p, size);
    memcpy (p, temp, size);
    ++swaps;

    /* walk down the tree */
    parent = child;
    p = c;
  }

  /* we can resize down here only since we need the temp element above */
  sc_array_resize (array, new_count);

  return swaps;
}

void               *
sc_array_index (sc_array_t * array, size_t index)
{
  SC_ASSERT (index < array->elem_count);

  return (void *) (array->array + (array->elem_size * index));
}

/* mempool routines */

static void        *(*obstack_chunk_alloc) (size_t) = sc_malloc;
static void         (*obstack_chunk_free) (void *) = sc_free;

sc_mempool_t       *
sc_mempool_new (size_t elem_size)
{
  sc_mempool_t       *mempool;

  SC_ASSERT (elem_size > 0);

  mempool = SC_ALLOC_ZERO (sc_mempool_t, 1);

  mempool->elem_size = elem_size;
  mempool->elem_count = 0;

  obstack_init (&mempool->obstack);
  sc_array_init (&mempool->freed, sizeof (void *));

  return mempool;
}

void
sc_mempool_destroy (sc_mempool_t * mempool)
{
  SC_ASSERT (mempool->elem_count == 0);

  sc_array_reset (&mempool->freed);
  obstack_free (&mempool->obstack, NULL);

  SC_FREE (mempool);
}

void
sc_mempool_reset (sc_mempool_t * mempool)
{
  sc_array_reset (&mempool->freed);
  obstack_free (&mempool->obstack, NULL);
  obstack_init (&mempool->obstack);
  mempool->elem_count = 0;
}

void               *
sc_mempool_alloc (sc_mempool_t * mempool)
{
  size_t              new_count;
  void               *ret;
  sc_array_t         *freed = &mempool->freed;

  ++mempool->elem_count;

  if (freed->elem_count > 0) {
    new_count = freed->elem_count - 1;
    ret = *(void **) sc_array_index (freed, new_count);
    sc_array_resize (freed, new_count);
  }
  else {
    ret = obstack_alloc (&mempool->obstack, (long) mempool->elem_size);
  }

#ifdef SC_DEBUG
  memset (ret, -1, mempool->elem_size);
#endif

  return ret;
}

void
sc_mempool_free (sc_mempool_t * mempool, void *elem)
{
  size_t              old_count;
  sc_array_t         *freed = &mempool->freed;

  SC_ASSERT (mempool->elem_count > 0);

#ifdef SC_DEBUG
  memset (elem, -1, mempool->elem_size);
#endif

  --mempool->elem_count;

  old_count = freed->elem_count;
  sc_array_resize (freed, old_count + 1);
  *(void **) sc_array_index (freed, old_count) = elem;
}

/* list routines */

sc_list_t          *
sc_list_new (sc_mempool_t * allocator)
{
  sc_list_t          *list;

  list = SC_ALLOC_ZERO (sc_list_t, 1);

  list->elem_count = 0;
  list->first = NULL;
  list->last = NULL;

  if (allocator != NULL) {
    SC_ASSERT (allocator->elem_size == sizeof (sc_link_t));
    list->allocator = allocator;
    list->allocator_owned = 0;
  }
  else {
    list->allocator = sc_mempool_new (sizeof (sc_link_t));
    list->allocator_owned = 1;
  }

  return list;
}

void
sc_list_destroy (sc_list_t * list)
{
  sc_list_reset (list);

  if (list->allocator_owned) {
    sc_mempool_destroy (list->allocator);
  }
  SC_FREE (list);
}

void
sc_list_init (sc_list_t * list, sc_mempool_t * allocator)
{
  list->elem_count = 0;
  list->first = NULL;
  list->last = NULL;

  SC_ASSERT (allocator != NULL);
  SC_ASSERT (allocator->elem_size == sizeof (sc_link_t));

  list->allocator = allocator;
  list->allocator_owned = 0;
}

void
sc_list_reset (sc_list_t * list)
{
  sc_link_t          *link;
  sc_link_t          *temp;

  link = list->first;
  while (link != NULL) {
    temp = link->next;
    sc_mempool_free (list->allocator, link);
    link = temp;
    --list->elem_count;
  }
  SC_ASSERT (list->elem_count == 0);

  list->first = list->last = NULL;
}

void
sc_list_unlink (sc_list_t * list)
{
  list->first = list->last = NULL;
  list->elem_count = 0;
}

void
sc_list_prepend (sc_list_t * list, void *data)
{
  sc_link_t          *link;

  link = sc_mempool_alloc (list->allocator);
  link->data = data;
  link->next = list->first;
  list->first = link;
  if (list->last == NULL) {
    list->last = link;
  }

  ++list->elem_count;
}

void
sc_list_append (sc_list_t * list, void *data)
{
  sc_link_t          *link;

  link = sc_mempool_alloc (list->allocator);
  link->data = data;
  link->next = NULL;
  if (list->last != NULL) {
    list->last->next = link;
  }
  else {
    list->first = link;
  }
  list->last = link;

  ++list->elem_count;
}

void
sc_list_insert (sc_list_t * list, sc_link_t * pred, void *data)
{
  sc_link_t          *link;

  SC_ASSERT (pred != NULL);

  link = sc_mempool_alloc (list->allocator);
  link->data = data;
  link->next = pred->next;
  pred->next = link;
  if (pred == list->last) {
    list->last = link;
  }

  ++list->elem_count;
}

void               *
sc_list_remove (sc_list_t * list, sc_link_t * pred)
{
  sc_link_t          *link;
  void               *data;

  if (pred == NULL) {
    return sc_list_pop (list);
  }

  SC_ASSERT (pred->next != NULL);

  link = pred->next;
  pred->next = link->next;
  data = link->data;
  if (list->last == link) {
    list->last = pred;
  }
  sc_mempool_free (list->allocator, link);

  --list->elem_count;
  return data;
}

void               *
sc_list_pop (sc_list_t * list)
{
  sc_link_t          *link;
  void               *data;

  SC_ASSERT (list->first != NULL);

  link = list->first;
  list->first = link->next;
  data = link->data;
  sc_mempool_free (list->allocator, link);
  if (list->first == NULL) {
    list->last = NULL;
  }

  --list->elem_count;
  return data;
}

/* hash table routines */

static const size_t sc_hash_minimal_size = (size_t) ((1 << 8) - 1);
static const size_t sc_hash_shrink_interval = (size_t) (1 << 8);

static void
sc_hash_maybe_resize (sc_hash_t * hash)
{
  size_t              i, j;
  size_t              new_size, new_count;
  sc_list_t          *old_list, *new_list;
  sc_link_t          *link, *temp;
  sc_array_t         *new_slots;
  sc_array_t         *old_slots = hash->slots;

  SC_ASSERT (old_slots->elem_count > 0);

  ++hash->resize_checks;
  if (hash->elem_count >= 4 * old_slots->elem_count) {
    new_size = 4 * old_slots->elem_count - 1;
  }
  else if (hash->elem_count <= old_slots->elem_count / 4) {
    new_size = old_slots->elem_count / 4 + 1;
    if (new_size < sc_hash_minimal_size) {
      return;
    }
  }
  else {
    return;
  }
  ++hash->resize_actions;

  /* allocate new slot array */
  new_slots = sc_array_new (sizeof (sc_list_t));
  sc_array_resize (new_slots, new_size);
  for (i = 0; i < new_size; ++i) {
    new_list = sc_array_index (new_slots, i);
    sc_list_init (new_list, hash->allocator);
  }

  /* go through the old slots and move data to the new slots */
  new_count = 0;
  for (i = 0; i < old_slots->elem_count; ++i) {
    old_list = sc_array_index (old_slots, i);
    link = old_list->first;
    while (link != NULL) {
      /* insert data into new slot list */
      j = hash->hash_fn (link->data) % new_size;
      new_list = sc_array_index (new_slots, j);
      sc_list_prepend (new_list, link->data);
      ++new_count;

      /* remove old list element */
      temp = link->next;
      sc_mempool_free (old_list->allocator, link);
      link = temp;
      --old_list->elem_count;
    }
    SC_ASSERT (old_list->elem_count == 0);
    old_list->first = old_list->last = NULL;
  }
  SC_ASSERT (new_count == hash->elem_count);

  /* replace old slots by new slots */
  sc_array_destroy (old_slots);
  hash->slots = new_slots;
}

sc_hash_t          *
sc_hash_new (sc_hash_function_t hash_fn,
             sc_equal_function_t equal_fn, sc_mempool_t * allocator)
{
  size_t              i;
  sc_hash_t          *hash;
  sc_list_t          *list;
  sc_array_t         *slots;

  hash = SC_ALLOC_ZERO (sc_hash_t, 1);

  if (allocator != NULL) {
    SC_ASSERT (allocator->elem_size == sizeof (sc_link_t));
    hash->allocator = allocator;
    hash->allocator_owned = 0;
  }
  else {
    hash->allocator = sc_mempool_new (sizeof (sc_link_t));
    hash->allocator_owned = 1;
  }

  hash->elem_count = 0;
  hash->resize_checks = 0;
  hash->resize_actions = 0;
  hash->hash_fn = hash_fn;
  hash->equal_fn = equal_fn;

  hash->slots = slots = sc_array_new (sizeof (sc_list_t));
  sc_array_resize (slots, sc_hash_minimal_size);
  for (i = 0; i < slots->elem_count; ++i) {
    list = sc_array_index (slots, i);
    sc_list_init (list, hash->allocator);
  }

  return hash;
}

void
sc_hash_destroy (sc_hash_t * hash)
{
  sc_hash_reset (hash);

  sc_array_destroy (hash->slots);
  if (hash->allocator_owned) {
    sc_mempool_destroy (hash->allocator);
  }

  SC_FREE (hash);
}

void
sc_hash_reset (sc_hash_t * hash)
{
  size_t              i, count;
  sc_list_t          *list;
  sc_array_t         *slots = hash->slots;

  if (hash->elem_count == 0) {
    return;
  }

  for (i = 0, count = 0; i < slots->elem_count; ++i) {
    list = sc_array_index (slots, i);
    count += list->elem_count;
    sc_list_reset (list);
  }
  SC_ASSERT (count == hash->elem_count);

  hash->elem_count = 0;
}

void
sc_hash_unlink (sc_hash_t * hash)
{
  size_t              i, count;
  sc_list_t          *list;
  sc_array_t         *slots = hash->slots;

  for (i = 0, count = 0; i < slots->elem_count; ++i) {
    list = sc_array_index (slots, i);
    count += list->elem_count;
    sc_list_unlink (list);
  }
  SC_ASSERT (count == hash->elem_count);

  hash->elem_count = 0;
}

void
sc_hash_unlink_destroy (sc_hash_t * hash)
{
  sc_array_destroy (hash->slots);
  if (hash->allocator_owned) {
    sc_mempool_destroy (hash->allocator);
  }

  SC_FREE (hash);
}

int
sc_hash_lookup (sc_hash_t * hash, void *v, void **found)
{
  size_t              hval;
  sc_list_t          *list;
  sc_link_t          *link;

  hval = hash->hash_fn (v) % hash->slots->elem_count;
  list = sc_array_index (hash->slots, hval);

  for (link = list->first; link != NULL; link = link->next) {
    /* check if an equal object is contained in the hash table */
    if (hash->equal_fn (link->data, v)) {
      if (found != NULL) {
        *found = link->data;
      }
      return 1;
    }
  }
  return 0;
}

int
sc_hash_insert_unique (sc_hash_t * hash, void *v, void **found)
{
  size_t              hval;
  sc_list_t          *list;
  sc_link_t          *link;

  hval = hash->hash_fn (v) % hash->slots->elem_count;
  list = sc_array_index (hash->slots, hval);

  for (link = list->first; link != NULL; link = link->next) {
    /* check if an equal object is already contained in the hash table */
    if (hash->equal_fn (link->data, v)) {
      if (found != NULL) {
        *found = link->data;
      }
      return 0;
    }
  }
  /* append new object to the list */
  sc_list_append (list, v);
  ++hash->elem_count;

  /* check for resize at specific intervals and return */
  if (hash->elem_count % hash->slots->elem_count == 0) {
    sc_hash_maybe_resize (hash);
  }
  return 1;
}

int
sc_hash_remove (sc_hash_t * hash, void *v, void **found)
{
  size_t              hval;
  sc_list_t          *list;
  sc_link_t          *link, *prev;

  hval = hash->hash_fn (v) % hash->slots->elem_count;
  list = sc_array_index (hash->slots, hval);

  prev = NULL;
  for (link = list->first; link != NULL; link = link->next) {
    /* check if an equal object is contained in the hash table */
    if (hash->equal_fn (link->data, v)) {
      if (found != NULL) {
        *found = link->data;
      }
      (void) sc_list_remove (list, prev);
      --hash->elem_count;

      /* check for resize at specific intervals and return */
      if (hash->elem_count % sc_hash_shrink_interval == 0) {
        sc_hash_maybe_resize (hash);
      }
      return 1;
    }
    prev = link;
  }
  return 0;
}

void
sc_hash_print_statistics (int log_priority, sc_hash_t * hash)
{
  size_t              i;
  double              a, sum, squaresum;
  double              divide, avg, sqr, std;
  sc_list_t          *list;
  sc_array_t         *slots = hash->slots;

  sum = 0.;
  squaresum = 0.;
  for (i = 0; i < slots->elem_count; ++i) {
    list = sc_array_index (slots, i);
    a = (double) list->elem_count;
    sum += a;
    squaresum += a * a;
  }
  SC_ASSERT ((size_t) sum == hash->elem_count);

  divide = (double) slots->elem_count;
  avg = sum / divide;
  sqr = squaresum / divide - avg * avg;
  std = sqrt (sqr);
  SC_LOGF (log_priority,
           "Hash size %lu avg %.3g std %.3g checks %lu %lu\n",
           (unsigned long) slots->elem_count, avg, std,
           (unsigned long) hash->resize_checks,
           (unsigned long) hash->resize_actions);
}

/* EOF sc_containers.c */