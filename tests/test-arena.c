#include "raii.h"
#include "test_assert.h"

int main(void) {
    arena_t arena = arena_init();
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));
    puts("arena_init finished\n");

    void *ptr = malloc_arena(arena, 18);
    ASSERT_EQ(10240, arena_capacity(arena));
    ASSERT_EQ(10368, arena_total(arena));
    puts("malloc_arena initialled\n");

    char *ptr2 = malloc_arena(arena, 11);
    arena_print(arena);
    void *ptr3 = malloc_arena(arena, 218);
    arena_print(arena);
    void *ptr4 = malloc_arena(arena, 1000);
    arena_print(arena);
    void *ptr5 = malloc_arena(arena, 1023);
    arena_print(arena);
    void *ptr6 = malloc_arena(arena, 1000);
    arena_print(arena);
    void *ptr7 = malloc_arena(arena, 2000);
    arena_print(arena);
    puts("");

    void *ptr8 = malloc_arena(arena, 4000);
    ASSERT_EQ(640, arena_capacity(arena));
    ASSERT_EQ(10368, arena_total(arena));
    puts("malloc_arena initialled ended\n");

    void *ptr9 = malloc_arena(arena, 4000);
    puts("malloc_arena total allocation increased\n");
    ASSERT_EQ(20736, arena_capacity(arena));
    ASSERT_EQ(24832, arena_total(arena));
    void *ptr10 = malloc_arena(arena, 4000);
    arena_print(arena);
    puts("malloc_arena again\n");

    arena_clear(arena);
    ASSERT_EQ(20736, arena_capacity(arena));
    arena_print(arena);
    puts("1. arena_clear\n");


    void *ptr11 = malloc_arena(arena, 6000);
    arena_print(arena);
    puts("malloc_arena again\n");

    arena_clear(arena);
    arena_print(arena);
    puts("2. arena_clear\n");

    void *ptr12 = malloc_arena(arena, 8000);
    arena_print(arena);
    puts("malloc_arena again\n");

    arena_clear(arena);
    ASSERT_EQ(arena_total(arena), arena_capacity(arena));
    puts("3. arena_clear\n");

    void *ptr13 = malloc_arena(arena, 1000);
    arena_print(arena);
    void *ptr14 = malloc_arena(arena, 1000);
    arena_print(arena);
    puts("malloc_arena again\n");

    arena_free(arena);
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));
    puts("arena_free\n");

    return 0;
}
