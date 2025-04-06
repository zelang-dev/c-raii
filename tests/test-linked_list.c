#include "linked.h"
#include "test_assert.h"

TEST(basics) {
    typedef struct {
        linked_t node;
        u32 data;
    } L;

    linked_t list = nil;
    L one = {nil, 1};
    L two = {nil, 2};
    L three = {nil, 3};
    L four = {nil, 4};
    L five = {nil, 5};

    link_append(&list, linker(&two)); // {2}
    link_append(&list, linker(&five)); // {2, 5}
    link_prepend(&list, linker(&one)); // {1, 2, 5}
    link_insert_before(&list, linker(&five), linker(&four)); // {1, 2, 4, 5}
    link_insert_after(&list, linker(&two), linker(&three)); // {1, 2, 3, 4, 5}

    // Traverse forwards.
    {
        u32 index = 1;
        foreach_link(item in &list) {
            ASSERT_EQ(index, link_iter(L, item)->data);
            index += 1;
        }
    }

    // Traverse backwards.
    {
        u32 index = 1;
        foreach_link_back(item in &list) {
            ASSERT_EQ((6 - index), link_iter(L, item)->data);
            index += 1;
        }
    }

    link_pop_first(&list); // {2, 3, 4, 5}
    link_pop(&list); // {2, 3, 4}
    link_remove(&list, linker(&three)); // {2, 4}

    ASSERT_EQ(2, link_first(L, &list)->data);
    ASSERT_EQ(4, link_last(L, &list)->data);
    ASSERT_XEQ(2, link_count(&list));

    return 0;
}

TEST(concatenation) {
    typedef struct {
        linked_t node;
        u32 data;
    } L;

    doubly_t list1 = doubly_create();
    doubly_t list2 = doubly_create();

    L one = {nil, 1};
    L two = {nil, 2};
    L three = {nil, 3};
    L four = {nil, 4};
    L five = {nil, 5};

    link_append(list1, linker(&one));
    link_append(list1, linker(&two));
    link_append(list2, linker(&three));
    link_append(list2, linker(&four));
    link_append(list2, linker(&five));

    link_concat(list1, list2);

    ASSERT_PTR(link_list_last(list1), linker(&five));
    ASSERT_XEQ(5, link_count(list1));
    ASSERT_NULL(link_list_first(list2));
    ASSERT_NULL(link_list_last(list2));
    ASSERT_XEQ(0, link_count(list2));

    // Traverse forwards.
    {
        u32 index = 1;
        foreach_link(item in list1) {
            ASSERT_EQ(index, link_iter(L, item)->data);
            index += 1;
        }
    }

    // Traverse backwards.
    {
        u32 index = 1;
        foreach_link_back(item in list1) {
            ASSERT_EQ((6 - index), link_iter(L, item)->data);
            index += 1;
        }
    }

    // Swap them back, this verifies that concatenating to an empty list works.
    link_concat(list2, list1);

    // Traverse forwards.
    {
        u32 index = 1;
        foreach_link(item in list2) {
            ASSERT_EQ(index, link_iter(L, item)->data);
            index += 1;
        }
    }

    // Traverse backwards.
    {
        u32 index = 1;
        foreach_link_back(item in list2) {
            ASSERT_EQ((6 - index), link_iter(L, item)->data);
            index += 1;
        }
    }

    return exit_scope();
}

TEST(list) {
    int result = 0;

    EXEC_TEST(basics);
    EXEC_TEST(concatenation);

    return result;
}

int main(int argc, char **argv) {
    TEST_FUNC(list());
}
