#include "raii.h"
#include "test_assert.h"

int main(void) {
    puts("\narena_init");
    arena_t arena = arena_init(0);
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));

    puts("\nmalloc_arena initialled");
    void *ptr = arena_alloc(arena, 18);
    ASSERT_EQ(10240, arena_capacity(arena));
    ASSERT_EQ(10258, arena_total(arena));

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
    ASSERT_EQ(986, arena_capacity(arena));
    ASSERT_EQ(10258, arena_total(arena));
    puts("arena_alloc initialled ended");

    puts("\nmalloc_arena(4000) cause total allocation to increase!");
    void *ptr9 = arena_alloc(arena, 4000);
    ASSERT_EQ(20626, arena_capacity(arena));
    ASSERT_EQ(24626, arena_total(arena));

    puts("\nmalloc_arena(4000) again");
    void *ptr10 = arena_alloc(arena, 4000);
    arena_print(arena);

    puts("\n1. arena_clear");
    arena_clear(arena);
    ASSERT_EQ(20626, arena_capacity(arena));
    arena_print(arena);

    puts("\nmalloc_aren(6000)a again");
    void *ptr11 = arena_alloc(arena, 6000);
    arena_print(arena);

    puts("\n2. arena_clear");
    arena_clear(arena);
    arena_print(arena);

    puts("\nmalloc_arena(8000) again");
    void *ptr12 = arena_alloc(arena, 8000);
    arena_print(arena);

    puts("\n3. arena_clear");
    arena_clear(arena);
    ASSERT_EQ(10626, arena_capacity(arena));

    puts("\nmalloc_arena(1000) x2 again");
    void *ptr13 = arena_alloc(arena, 1000);
    arena_print(arena);
    void *ptr14 = arena_alloc(arena, 1000);
    arena_print(arena);

    puts("\nmalloc_arena(61000)");
    void *ptr15 = arena_alloc(arena, 61000);
    arena_print(arena);
    ASSERT_EQ(34994, arena_capacity(arena));
    ASSERT_EQ(95994, arena_total(arena));

    puts("\nmalloc_arena(7000)");
    void *ptr16 = arena_alloc(arena, 7000);
    arena_print(arena);

    puts("\narena_free");
    arena_free(arena);
    ASSERT_EQ(0, arena_capacity(arena));
    ASSERT_EQ(0, arena_total(arena));
    puts("");

    return 0;
}
