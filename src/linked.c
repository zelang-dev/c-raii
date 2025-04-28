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
        list->tail = new_node;
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
        list->head = new_node;
    }
    existing_node->prev = new_node;
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

RAII_INLINE void link_prepend(linked_t *list, doubly_t new_node) {
    if (list->head) {
        linked_t *head = list->head;
        // Insert before head.
        link_insert_before(list, head, new_node);
    } else {
        // Empty list.
        list->head = new_node;
        list->tail = new_node;
        new_node->prev = nullptr;
        new_node->next = nullptr;
        list->count++;
    }
}

RAII_INLINE void link_append(linked_t *list, doubly_t new_node) {
    if (list->tail) {
        linked_t *tail = list->tail;
        // Insert after tail.
        link_insert_after(list, tail, new_node);
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
        list->head = node->next;
    }

    if (node->next) {
        linked_t *next_node = node->next;
        // Intermediate node.
        next_node->prev = node->prev;
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
