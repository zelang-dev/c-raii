#include "raii.h"
#include "test_assert.h"

int main(void) {
    arena_t arena = arena_new();
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));

    void *ptr = arena_alloc(arena, 18);
    ASSERT_EQ(10240, arena_capacity(arena));
    ASSERT_EQ(10752, arena_total(arena));

    char *ptr2 = arena_alloc(arena, 11);
    arena_print(arena);
    void *ptr3 = arena_alloc(arena, 218);
    arena_print(arena);
    void *ptr4 = arena_alloc(arena, 1000);
    arena_print(arena);
    void *ptr5 = arena_alloc(arena, 1023);
    arena_print(arena);
    void *ptr6 = arena_alloc(arena, 1000);
    arena_print(arena);
    void *ptr7 = arena_alloc(arena, 2000);
    arena_print(arena);

    void *ptr8 = arena_alloc(arena, 4000);
    ASSERT_EQ(512, arena_capacity(arena));
    ASSERT_EQ(10752, arena_total(arena));

    void *ptr9 = arena_alloc(arena, 4000);
    ASSERT_EQ(10240, arena_capacity(arena));
    ASSERT_EQ(25344, arena_total(arena));

    void *ptr10 = arena_alloc(arena, 4000);
    arena_free(arena);
    ASSERT_EQ(6144, arena_capacity(arena));

    arena_dispose(arena);
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));

    return 0;
}
