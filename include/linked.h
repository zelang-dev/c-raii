#ifndef _LINKED_H_
#define _LINKED_H_
#include "raii.h"

typedef struct linked_s linked_t;
typedef struct linked_iter_s linked_iter_t;

/* A doubly-linked list has a pair of pointers to both the head and
tail of the list. List elements have pointers to both the previous
and next elements in the sequence.

The list can be traversed both forward and backward. Some operations
that take linear O(n) time with a singly-linked list can be done
without traversal in constant O(1) time with a doubly-linked list:

** Removing an element.
** Inserting a new element before an existing element.
** Pushing or popping an element from the end of the list.
*/
typedef linked_t *doubly_t;
struct linked_s {
    raii_type type;
    raii_type item_type;
    doubly_t prev;
    doubly_t next;
    doubly_t head;
    doubly_t tail;
    size_t count;
    template value[1];
};

C_API doubly_t doubly_create(void);
C_API void link_insert_after(linked_t *list, doubly_t, doubly_t);
C_API void link_insert_before(linked_t *list, doubly_t, doubly_t);

// Concatenate list2 onto the end of list1, removing all entries from the former.
//
// Arguments:
//     list1: the list to concatenate onto
//     list2: the list to be concatenated
C_API void link_concat(linked_t *list1, linked_t *list2);

// Insert a new node at the beginning of the list.
//
// Arguments:
//     new: Pointer to the new node to insert.
C_API void link_prepend(linked_t *list, doubly_t);

// Insert a new node at the end of the list.
//
// Arguments:
//     new: Pointer to the new node to insert.
C_API void link_append(linked_t *list, doubly_t);

// Remove a node from the list.
//
// Arguments:
//     node: Pointer to the node to be removed.
C_API void link_remove(linked_t *list, doubly_t);

// Remove and return the last node in the list.
//
// Returns:
//     A pointer to the last node in the list.
C_API doubly_t link_pop(linked_t *list);

// Remove and return the first node in the list.
//
// Returns:
//     A pointer to the first node in the list.
C_API doubly_t link_pop_first(linked_t *list);

C_API size_t link_count(linked_t *list);

C_API linked_iter_t *link_iter_create(linked_t *, bool forward);
C_API linked_iter_t *link_next(linked_iter_t *);
C_API template link_value(linked_iter_t *);
C_API linked_t *link_item(linked_iter_t *);
C_API doubly_t link_list_first(linked_t *);
C_API doubly_t link_list_last(linked_t *);

#define link_iter(type, item) ((type*)link_item(item))
#define link_first(type, list) ((type*)link_list_first(list))
#define link_last(type, list) ((type*)link_list_last(list))
#define linker(list) ((doubly_t)(list))

#define foreach_in_link(X, S)   linked_iter_t *(X); \
    for(X = link_iter_create((doubly_t)(S), true); X != nullptr; X = link_next(X))

#define foreach_in_link_r(X, S) linked_iter_t *(X); \
    for(X = link_iter_create((doubly_t)(S), false); X != nullptr; X = link_next(X))

#define foreach_link(...) foreach_xp(foreach_in_link, (__VA_ARGS__))
#define foreach_link_back(...) foreach_xp(foreach_in_link_r, (__VA_ARGS__))

#endif