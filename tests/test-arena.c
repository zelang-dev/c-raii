#include "raii.h"
#include "test_assert.h"

int main(void) {
    puts("\narena_init");
    arena_t arena = arena_init();
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));

    puts("\nmalloc_arena initialled");
    void *ptr = malloc_arena(arena, 18);
    ASSERT_EQ(10240, arena_capacity(arena));
    ASSERT_EQ(10368, arena_total(arena));

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
    void *ptr8 = malloc_arena(arena, 4000);
    ASSERT_EQ(640, arena_capacity(arena));
    ASSERT_EQ(10368, arena_total(arena));
    puts("malloc_arena initialled ended");

    puts("\nmalloc_arena(4000) cause total allocation to increase!");
    void *ptr9 = malloc_arena(arena, 4000);
    ASSERT_EQ(20736, arena_capacity(arena));
    ASSERT_EQ(24832, arena_total(arena));

    puts("\nmalloc_arena(4000) again");
    void *ptr10 = malloc_arena(arena, 4000);
    arena_print(arena);

    puts("\n1. arena_clear");
    arena_clear(arena);
    ASSERT_EQ(20736, arena_capacity(arena));
    arena_print(arena);

    puts("\nmalloc_aren(6000)a again");
    void *ptr11 = malloc_arena(arena, 6000);
    arena_print(arena);

    puts("\n2. arena_clear");
    arena_clear(arena);
    arena_print(arena);

    puts("\nmalloc_arena(8000) again");
    void *ptr12 = malloc_arena(arena, 8000);
    arena_print(arena);

    puts("\n3. arena_clear");
    arena_clear(arena);
    arena_print(arena);
    ASSERT_EQ(arena_total(arena), arena_capacity(arena));

    puts("\nmalloc_arena(1000) x2 again");
    void *ptr13 = malloc_arena(arena, 1000);
    arena_print(arena);
    void *ptr14 = malloc_arena(arena, 1000);
    arena_print(arena);

    puts("\narena_free");
    arena_free(arena);
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));
    puts("");

    return 0;
}
