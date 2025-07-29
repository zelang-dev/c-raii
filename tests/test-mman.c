#include "raii.h"
#include "test_assert.h"

const char *map_file_name = "map_file.dat";

int test_anon_map_readwrite()
{
    void *map = mmap(NULL, 1024, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap (MAP_ANONYMOUS, PROT_READ | PROT_WRITE) returned unexpected error: %d\n", errno);
        return -1;
    }

    *((unsigned char *)map) = 1;

    int result = munmap(map, 1024);

    if (result != 0)
        printf("munmap (MAP_ANONYMOUS, PROT_READ | PROT_WRITE) returned unexpected error: %d\n", errno);

    return result;
}

int test_anon_map_readonly()
{
    int result = 1;
    void *map = mmap(NULL, 1024, PROT_READ,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap (MAP_ANONYMOUS, PROT_READ) returned unexpected error: %d\n", errno);
        return -1;
    }

    try {
        *((unsigned char *)map) = 1;
    } catch_any {
        result = munmap(map, 1024);
        if (result != 0)
            printf("munmap (MAP_ANONYMOUS, PROT_READ) returned unexpected error: %d\n", errno);
    }

    return result;
}

int test_anon_map_writeonly()
{
    void *map = mmap(NULL, 1024, PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap (MAP_ANONYMOUS, PROT_WRITE) returned unexpected error: %d\n", errno);
        return -1;
    }

    *((unsigned char *)map) = 1;

    int result = munmap(map, 1024);

    if (result != 0)
        printf("munmap (MAP_ANONYMOUS, PROT_WRITE) returned unexpected error: %d\n", errno);

    return result;
}

int test_anon_map_readonly_nowrite()
{
    void *map = mmap(NULL, 1024, PROT_READ,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap (MAP_ANONYMOUS, PROT_READ) returned unexpected error: %d\n", errno);
        return -1;
    }

    if (*((unsigned char *)map) != 0)
        printf("test_anon_map_readonly_nowrite (MAP_ANONYMOUS, PROT_READ) returned unexpected value: %d\n",
               (int)*((unsigned char *)map));

    int result = munmap(map, 1024);

    if (result != 0)
        printf("munmap (MAP_ANONYMOUS, PROT_READ) returned unexpected error: %d\n", errno);

    return result;
}

int test_file_map_readwrite()
{
    mode_t mode = S_IRUSR | S_IWUSR;
    int o = open(map_file_name, O_TRUNC | O_BINARY | O_RDWR | O_CREAT, mode);

    void *map = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE, o, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap returned unexpected error: %d\n", errno);
        return -1;
    }

    *((unsigned char *)map) = 1;

    int result = munmap(map, 1024);

    if (result != 0)
        printf("munmap returned unexpected error: %d\n", errno);

    close(o);

    /*TODO: get file info and content and compare it with the sources conditions */
    unlink(map_file_name);

    return result;
}

int test_file_map_mlock_munlock()
{
    const size_t map_size = 1024;

    int result = 0;
    mode_t mode = S_IRUSR | S_IWUSR;
    int o = open(map_file_name, O_TRUNC | O_BINARY | O_RDWR | O_CREAT, mode);
    if (o == -1)
    {
        printf("unable to create file %s: %d\n", map_file_name, errno);
        return -1;
    }

    void *map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, o, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap returned unexpected error: %d\n", errno);
        result = -1;
        goto done_close;
    }

    if (mlock(map, map_size) != 0)
    {
        printf("mlock returned unexpected error: %d\n", errno);
        result = -1;
        goto done_munmap;
    }

    *((unsigned char *)map) = 1;

    if (munlock(map, map_size) != 0)
    {
        printf("munlock returned unexpected error: %d\n", errno);
        result = -1;
    }

done_munmap:
    result = munmap(map, map_size);

    if (result != 0)
        printf("munmap returned unexpected error: %d\n", errno);

done_close:
    close(o);

    unlink(map_file_name);
    return result;
}

#define BUFSIZE 8

int test_mlock_valid_address()
{
    int result;
    long page_size;
    void *page_ptr;

    page_size = getpagesize();
    if (page_size == -1) {
        perror("An error occurs when calling getpagesize()");
        return -1;
    }

    page_ptr = (void *)(LONG_MAX - (LONG_MAX % page_size));
    result = mlock(&page_ptr, BUFSIZE);

    if (result == -1 && errno == ENOMEM) {
        result = 0;
    } else if (errno == EPERM) {
        printf
        ("You don't have permission to lock your address space.\nTry to rerun this test as root.\n");
        result = -1;
    }

    if (munlock(&page_ptr, BUFSIZE) != 0) {
        printf("munlock returned unexpected error: %d\n", errno);
        result = -1;
    }

    return result;
}

int test_mlock_page()
{
    int result;
    long page_size;
    void *ptr;

    page_size = getpagesize();
    if (page_size == -1) {
        perror("An error occurs when calling getpagesize()");
        return -1;
    }

    ptr = malloc(page_size);
    if (ptr == NULL) {
        printf("Can not allocate memory.\n");
        return -1;
    }

    result = mlock(ptr, page_size - 1);
    if (result == 0) {
        result = 0;
    } else if (result == -1 && errno == EINVAL) {
        printf
        ("mlock() requires that addr be a multiple of {PAGESIZE}.\n");
        result = 0;
    } else if (errno == EPERM) {
        printf
        ("You don't have permission to lock your address space.\nTry to rerun this test as root.\n");
        result = -1;
    } else if (result != -1) {
        printf("mlock() returns a value of %i instead of 0 or 1.\n",
               result);
        perror("mlock");
        result = -1;
    } else {
        perror("Unexpected error");
        result = -1;
    }

    if (munlock(ptr, page_size - 1) != 0) {
        printf("munlock returned unexpected error: %d\n", errno);
        result = -1;
    }

    free(ptr);

    return result;
}

int test_file_map_msync()
{
    const size_t map_size = 1024;

    int result = 0;
    mode_t mode = S_IRUSR | S_IWUSR;
    int o = open(map_file_name, O_TRUNC | O_BINARY | O_RDWR | O_CREAT, mode);
    if (o == -1)
    {
        printf("unable to create file %s: %d\n", map_file_name, errno);
        return -1;
    }

    void *map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, o, 0);
    if (map == MAP_FAILED)
    {
        printf("mmap returned unexpected error: %d\n", errno);
        result = -1;
        goto done_close;
    }

    *((unsigned char *)map) = 1;

    if (msync(map, map_size, MS_SYNC) != 0)
    {
        printf("msync returned unexpected error: %d\n", errno);
        result = -1;
    }

    result = munmap(map, map_size);

    if (result != 0)
        printf("munmap returned unexpected error: %d\n", errno);

done_close:
    close(o);

    unlink(map_file_name);
    return result;
}

int test_map_mprotect() {
    char *p;
    int result = -1;
    long pagesize = getpagesize();

    if (pagesize == -1)
        raii_panic("sysconf");

    /* Allocate a buffer aligned on a page boundary;
       initial protection is PROT_READ | PROT_WRITE. */
    void *buffer = memalign(pagesize, 4 * pagesize);
    if (buffer == NULL)
        raii_panic("memalign");

    if (mprotect(buffer, pagesize, PROT_READ) == -1)
        raii_panic("mprotect");

    try {
        for (p = buffer; ; )
            *(p++) = 'a';

        printf("Loop completed\n");     /* Should never happen */
#if defined(__APPLE__) || defined(__MACH__)
    } catch (sig_bus) {
#else
    } catch (sig_segv) {
#endif
        result = 0;
    }

#if !defined(__APPLE__) || !defined(__MACH__)
        free(buffer);
#endif
    return result;
}

int main(int argc, char **argv)
{
    int result = 0;

#if !defined(__powerpc64__)
    EXEC_TEST(anon_map_readwrite);
    // NOTE: this test must cause an access violation exception
    EXEC_TEST(anon_map_readonly);
    EXEC_TEST(anon_map_readonly_nowrite);
    EXEC_TEST(anon_map_writeonly);

#ifdef _WIN32
    EXEC_TEST(file_map_readwrite);
    EXEC_TEST(file_map_mlock_munlock);
    EXEC_TEST(file_map_msync);
#endif
    EXEC_TEST(mlock_valid_address);
    EXEC_TEST(mlock_page);
    EXEC_TEST(map_mprotect);
#endif

    return result;
}
