#include "linked.h"

struct linked_iter_s {
    raii_type type;
    bool forward;
    doubly_t item;
};

RAII_INLINE doubly_t doubly_create(void) {
    return (doubly_t)calloc_local(1, sizeof(linked_t));
}

RAII_INLINE void link_insert_after(linked_t *list, doubly_t existing, doubly_t new) {
    new->prev = existing;
    if (existing->next) {
        doubly_t next = existing->next;
        // Intermediate node.
        new->next = next;
        next->prev = new;
    } else {
        // Last element of the list.
        new->next = nullptr;
        list->tail = new;
    }
    existing->next = new;
    list->count++;
}

RAII_INLINE void link_insert_before(linked_t *list, doubly_t existing, doubly_t new) {
    new->next = existing;
    if (existing->prev) {
        doubly_t prev = existing->prev;
        // Intermediate node.
        new->prev = prev;
        prev->next = new;
    } else {
        // First element of the list.
        new->prev = nullptr;
        list->head = new;
    }
    existing->prev = new;
    list->count++;
}

RAII_INLINE void link_concat(linked_t *list1, linked_t *list2) {
    if (!list2->head)
        return;

    u32 total = list1->count + list2->count;
    linked_t *l2_first = list2->head;
    if (list1->tail) {
        linked_t *l1_last = list1->tail;
        l1_last->next = list2->head;
        l2_first->prev = list1->tail;
    } else {
        // list1 was empty
        list1->head = list2->head;
    }

    list1->tail = list2->tail;
    list2->head = nullptr;
    list2->tail = nullptr;
    list1->count = total;
    list2->count = 0;
}

RAII_INLINE void link_prepend(linked_t *list, doubly_t new) {
    if (list->head) {
        linked_t *head = list->head;
        // Insert before head.
        link_insert_before(list, head, new);
    } else {
        // Empty list.
        list->head = new;
        list->tail = new;
        new->prev = nullptr;
        new->next = nullptr;
        list->count++;
    }
}

RAII_INLINE void link_append(linked_t *list, doubly_t new) {
    if (list->tail) {
        linked_t *tail = list->tail;
        // Insert after tail.
        link_insert_after(list, tail, new);
    } else {
        // Empty list.
       link_prepend(list, new);
    }
}

RAII_INLINE void link_remove(linked_t *list, doubly_t node) {
    if (node->prev) {
        linked_t *prev = node->prev;
        // Intermediate node.
        prev->next = node->next;
    } else {
        // First element of the list.
        list->head = node->next;
    }

    if (node->next) {
        linked_t *next = node->next;
        // Intermediate node.
        next->prev = node->prev;
    } else {
        // Last element of the list.
        list->tail = node->prev;
    }

    list->count--;
}

RAII_INLINE doubly_t link_pop(linked_t *list) {
    if (!list->tail)
        return nullptr;

    doubly_t tail = list->tail;
    link_remove(list, tail);

    return tail;
}

RAII_INLINE doubly_t link_pop_first(linked_t *list) {
    if (!list->head)
        return nullptr;

    doubly_t head = list->head;
    link_remove(list, head);

    return head;
}

RAII_INLINE doubly_t link_list_first(linked_t *list) {
    if (!list->head)
        return nullptr;

    return list->head;
}

RAII_INLINE doubly_t link_list_last(linked_t *list) {
    if (!list->tail)
        return nullptr;

    return list->tail;
}

RAII_INLINE size_t link_count(linked_t *list) {
    if (list)
        return list->count;

    return 0;
}

linked_iter_t *link_iter_create(linked_t *list, bool forward) {
    if (list) {
        linked_iter_t *iterator = (linked_iter_t *)try_calloc(1, sizeof(linked_iter_t));
        doubly_t item = forward ? list->head : list->tail;
        iterator->item = item;
        iterator->forward = forward;
        iterator->type = RAII_LINKED_ITER;

        return iterator;
    }

    return nullptr;
}

linked_iter_t *link_next(linked_iter_t *iterator) {
    if (iterator) {
        linked_t *item;

        item = iterator->forward ? iterator->item->next : iterator->item->prev;
        if (item) {
            iterator->item = item;
            return iterator;
        } else {
            ZE_FREE(iterator);
            return nullptr;
        }
    }

    return nullptr;
}

RAII_INLINE template link_value(linked_iter_t *iterator) {
    if (iterator)
        return *iterator->item->value;

    return raii_values_empty->valued;
}

RAII_INLINE linked_t *link_item(linked_iter_t *iterator) {
    if (iterator)
        return iterator->item;

    return nullptr;
}
