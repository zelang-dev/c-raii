#include "raii.h"
#include "test_assert.h"

int main(void) {
    arena_t arena = arena_new();
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));
    puts("");

    void *ptr = arena_alloc(arena, 18);
    ASSERT_EQ(10240, arena_capacity(arena));
    ASSERT_EQ(10496, arena_total(arena));

    puts("");
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
    puts("");

    void *ptr8 = arena_alloc(arena, 4000);
    ASSERT_EQ(640, arena_capacity(arena));
    ASSERT_EQ(10496, arena_total(arena));
    puts("");

    void *ptr9 = arena_alloc(arena, 4000);
    ASSERT_EQ(20736, arena_capacity(arena));
    ASSERT_EQ(24960, arena_total(arena));
    void *ptr10 = arena_alloc(arena, 4000);
    arena_free(arena);
    ASSERT_EQ(16640, arena_capacity(arena));
    arena_print(arena);
    puts("");

    void *ptr11 = arena_alloc(arena, 6000);
    arena_print(arena);
    arena_free(arena);
    void *ptr12 = arena_alloc(arena, 8000);
    arena_print(arena);
    arena_free(arena);
    void *ptr13 = arena_alloc(arena, 1000);
    arena_print(arena);
    void *ptr14 = arena_alloc(arena, 1000);
    arena_print(arena);
    puts("");

    arena_dispose(arena);
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));
    puts("");

    return 0;
}
