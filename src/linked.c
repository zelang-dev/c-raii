#include "linked.h"

struct linked_iter_s {
    raii_type type;
    bool forward;
    doubly_t item;
};

RAII_INLINE doubly_t doubly_create(void) {
    return (doubly_t)calloc_local(1, sizeof(linked_t));
}

RAII_INLINE void link_insert_after(linked_t *list, doubly_t existing_node, doubly_t new_node) {
    new_node->prev = existing_node;
    if (existing_node->next) {
        doubly_t next_node = existing_node->next;
        // Intermediate node.
        new_node->next = next_node;
        next_node->prev = new_node;
    } else {
        // Last element of the list.
        new_node->next = nullptr;
        list->last = new_node;
    }
    existing_node->next = new_node;
    list->count++;
}

RAII_INLINE void link_insert_before(linked_t *list, doubly_t existing_node, doubly_t new_node) {
    new_node->next = existing_node;
    if (existing_node->prev) {
        doubly_t prev_node = existing_node->prev;
        // Intermediate node.
        new_node->prev = prev_node;
        prev_node->next = new_node;
    } else {
        // First element of the list.
        new_node->prev = nullptr;
        list->first = new_node;
    }
    existing_node->prev = new_node;
    list->count++;
}

RAII_INLINE void link_concat(linked_t *list1, linked_t *list2) {
    if (!list2->first)
        return;

    u32 total = list1->count + list2->count;
    linked_t *l2_first = list2->first;
    if (list1->last) {
        linked_t *l1_last = list1->last;
        l1_last->next = list2->first;
        l2_first->prev = list1->last;
    } else {
        // list1 was empty
        list1->first = list2->first;
    }

    list1->last = list2->last;
    list2->first = nullptr;
    list2->last = nullptr;
    list1->count = total;
    list2->count = 0;
}

RAII_INLINE void link_prepend(linked_t *list, doubly_t new_node) {
    if (list->first) {
        linked_t *first = list->first;
        // Insert before first.
        link_insert_before(list, first, new_node);
    } else {
        // Empty list.
        list->first = new_node;
        list->last = new_node;
        new_node->prev = nullptr;
        new_node->next = nullptr;
        list->count++;
    }
}

RAII_INLINE void link_append(linked_t *list, doubly_t new_node) {
    if (list->last) {
        linked_t *last = list->last;
        // Insert after last.
        link_insert_after(list, last, new_node);
    } else {
        // Empty list.
       link_prepend(list, new_node);
    }
}

RAII_INLINE void link_remove(linked_t *list, doubly_t node) {
    if (node->prev) {
        linked_t *prev_node = node->prev;
        // Intermediate node.
        prev_node->next = node->next;
    } else {
        // First element of the list.
        list->first = node->next;
    }

    if (node->next) {
        linked_t *next_node = node->next;
        // Intermediate node.
        next_node->prev = node->prev;
    } else {
        // Last element of the list.
        list->last = node->prev;
    }

    list->count--;
}

RAII_INLINE doubly_t link_pop(linked_t *list) {
    if (!list->last)
        return nullptr;

    doubly_t last = list->last;
    link_remove(list, last);

    return last;
}

RAII_INLINE doubly_t link_pop_first(linked_t *list) {
    if (!list->first)
        return nullptr;

    doubly_t first = list->first;
    link_remove(list, first);

    return first;
}

RAII_INLINE doubly_t link_list_first(linked_t *list) {
    if (!list->first)
        return nullptr;

    return list->first;
}

RAII_INLINE doubly_t link_list_last(linked_t *list) {
    if (!list->last)
        return nullptr;

    return list->last;
}

RAII_INLINE size_t link_count(linked_t *list) {
    if (list)
        return list->count;

    return 0;
}

linked_iter_t *link_iter_create(linked_t *list, bool forward) {
    if (list) {
        linked_iter_t *iterator = (linked_iter_t *)try_calloc(1, sizeof(linked_iter_t));
        doubly_t item = forward ? list->first : list->last;
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

RAII_INLINE value_t link_value(linked_iter_t *iterator) {
    if (iterator)
        return *iterator->item->value;

    return raii_values_empty->valued;
}

RAII_INLINE linked_t *link_item(linked_iter_t *iterator) {
    if (iterator)
        return iterator->item;

    return nullptr;
}
