# c-raii

[![windows & linux & macos](https://github.com/zelang-dev/c-raii/actions/workflows/ci.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci.yml)
[![centos](https://github.com/zelang-dev/c-raii/actions/workflows/ci_centos.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_centos.yml)
[![various cpu's](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu.yml)

An robust high-level **Defer**, _RAII_ implementation for `C89`, automatic memory safety, _smarty!_

* [Synopsis](#synopsis)
  * [There is _1 way_ to create an smart memory pointer.](#there-is-1-way-to-create-an-smart-memory-pointer)
  * [The following _malloc/calloc_ wrapper functions are used to get an raw memory pointer.](#the-following-malloccalloc-wrapper-functions-are-used-to-get-an-raw-memory-pointer)
  * [Thereafter, an smart pointer can be use with these _raii_* functions](#thereafter-an-smart-pointer-can-be-use-with-these-raii-functions)
  * [Using `thread local storage` for an default smart pointer, the following functions always available.](#using-thread-local-storage-for-an-default-smart-pointer-the-following-functions-always-available)
  * [Fully automatic memory safety, using `guard/unguarded/guarded` macro.](#fully-automatic-memory-safety-using-guardunguardedguarded-macro)
* [Installation](#installation)
* [Contributing](#contributing)
* [License](#license)

This branch, version `2.x` of **c-raii**, has an expanded feature set. It differs greatly from [1.x](https://github.com/zelang-dev/c-raii/blob/1.x/) to be in line with the fact that most ordinary `C` _libraries_ in-use will need _refactoring_ aka **rewrite**, to be use effectually with _memory safety_ as first class.

The features now encompass most things you might find in higher level languages, specificity **ease of use**, _with the same old_ **C89**. Not new concepts, you'll find most implicit in every _program/application_, if not explicit, now available in additional header files.

* dynamic data-structures [vector.h](https://github.com/zelang-dev/c-raii/blob/main/include/vector.h), [hashtable.h](https://github.com/zelang-dev/c-raii/blob/main/include/hashtable.h) and [map.h](https://github.com/zelang-dev/c-raii/blob/main/include/map.h).
* parsers [url_http.h](https://github.com/zelang-dev/c-raii/blob/main/include/url_http.h) and [json.h](https://github.com/zelang-dev/c-raii/blob/main/include/json.h).
* control flow constructs, _threads, coroutines, and channels_ [coro.h](https://github.com/zelang-dev/c-raii/blob/main/include/coro.h) and [channel.h](https://github.com/zelang-dev/c-raii/blob/main/include/channel.h).
* string manipulation/handling [swar.h](https://github.com/zelang-dev/c-raii/blob/main/include/swar.h).
* access/manipulate an individual integer bits, with string representation [bitset.h](https://github.com/zelang-dev/c-raii/blob/main/include/bitset.h).
* runtime code [reflection.h](https://github.com/zelang-dev/c-raii/blob/main/include/reflection.h).

> See [tests](https://github.com/zelang-dev/c-raii/blob/main/tests) and [examples](https://github.com/zelang-dev/c-raii/blob/main/examples) **folder** for _usage_.

This library has come full circle from it's initial aim at being _decoupled_ from [c-coroutine](https://zelang-dev.github.io/c-coroutine) to _bridging_ it back with different startup strategy behavior. Must either **`#define USE_CORO`** or call **`coro_start(main_func, argc, argv, 0)`**. When active **C** become whatever _high level_ language you can imagine that offers _automatic memory management_, otherwise no coroutine support, program will **crash** calling any specific coroutine only functions.

The one _essential_ thing that all languages must control and _redefine_ doing the compiler creation process, an _function_ **"creation/signature/entrance/exit"** _process_. The whole **"signature/entrance/exit"** process here is under **C-RAII** _coroutine_ control.

**_Everything that follows at point is from version `1.x`_**

This library has been decoupled from [c-coroutine](https://zelang-dev.github.io/c-coroutine) to be independently developed.

In the effort to find uniform naming of terms, various other packages was discovered [Except](https://github.com/meaning-matters/Except), and [exceptions-and-raii-in-c](https://github.com/psevon/exceptions-and-raii-in-c). Choose to settle in on [A defer mechanism for C](https://gustedt.wordpress.com/2020/12/14/a-defer-mechanism-for-c/), an upcoming C standard compiler feature. It's exactly this library's working design and concepts addressed in [c-coroutine](https://github.com/zelang-dev/c-coroutine).

This library uses an custom version of [rpmalloc](https://github.com/mjansson/rpmalloc) for **malloc/heap** allocation, not **C11** compiler `thread local` dependant, nor **C++** focus, _removed_. The customization is merged with required [cthread](https://github.com/zelang-dev/cthread) for **C11** _thread like emulation_.

* The behavior here is as in other _languages_ **Go's** [defer](https://go.dev/ref/spec#Defer_statements), **Zig's** [defer](https://ziglang.org/documentation/master/#defer), **Swift's** [defer](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/statements/#Defer-Statement), even **Rust** has [multi defer crates](https://crates.io/keywords/defer) there are other **borrow checker** issues - [A defer discussion](https://internals.rust-lang.org/t/a-defer-discussion/20387).

> As a side benefit, just including a single `#include "raii.h"` will make your **Linux** only application **Windows** compatible, see `work-steal.c` in [examples](https://github.com/zelang-dev/c-raii/tree/main/examples) folder, it's from [Complementary Concurrency Programs for course "Linux Kernel Internals"](https://github.com/sysprog21/concurrent-programs), _2 minor changes_, using macro `make_atomic`.

> All aspect of this library as been incorporated into an **c++11** like _threading model_ as outlined in [std::async](https://cplusplus.com/reference/future/async/), any thread launched with signature `thrd_async(fn, num_of_args, ...)` has function executed within `guard` block as further described below, all allocations is automatically cleaned up and `defer` statements **run**.

The C++ version from <https://cplusplus.com/reference/future/future/wait/>

<table>
<tr>
<th>RAII C89</th>
<th>C++11</th>
</tr>
<tr>
<td>

```c
#include "raii.h"

// a non-optimized way of checking for prime numbers:
void *is_prime(args_t arg) {
    int i, x = arg[0].integer;
    for (i = 2; i < x; ++i) if (x % i == 0) return $(false);
    return $(true);
}

int main(int argc, char **argv) {
    int prime = 194232491;
    // call function asynchronously:
    future fut = thrd_async(is_prime, 1, prime);

    printf("checking...\n");
    thrd_wait(fut, thrd_yield);

    printf("\n194232491 ");
    if (thrd_get(fut).boolean) // guaranteed to be ready (and not block) after wait returns
        printf("is prime.\n");
    else
        printf("is not prime.\n");
}
```

</td>
<td>

```c++
// future::wait
#include <iostream>       // std::cout
#include <future>         // std::async, std::future
#include <chrono>         // std::chrono::milliseconds

// a non-optimized way of checking for prime numbers:
bool is_prime (int x) {
  for (int i=2; i<x; ++i) if (x%i==0) return false;
  return true;
}

int main ()
{
  // call function asynchronously:
  std::future<bool> fut = std::async (is_prime,194232491);

  std::cout << "checking...\n";
  fut.wait();

  std::cout << "\n194232491 ";
  if (fut.get())      // guaranteed to be ready (and not block) after wait returns
    std::cout << "is prime.\n";
  else
    std::cout << "is not prime.\n";

  return 0;
}
```

</td>
</tr>
</table>

The planned implementation from [defer reference implementation for C](https://gustedt.gitlabpages.inria.fr/defer/):

<table>
<tr>
<th>RAII C89</th>
<th>Planned C11</th>
</tr>
<tr>
<td>

<pre><code>
// includes "cthread.h" emulated C11 threads
#include "raii.h"

int main(int argc, char **argv)
guard {
    // Panic if p == NULL
    // Automatically _defer(free, p)
    void *p = _malloc(25);
    void *q = _calloc(1, 25);

    if (mtx_lock(&mut)==thrd_error) break;
    _defer(mtx_unlock, &mut);

    // all resources acquired
} unguarded(0);
</code></pre>
</td>

<td>
<pre><code>
guard {
  void * const p = malloc(25);
  if (!p) break;
  defer free(p);

  void * const q = malloc(25);
  if (!q) break;
  defer free(q);

  if (mtx_lock(&mut)==thrd_error) break;
  defer mtx_unlock(&mut);

  // all resources acquired
}
</code></pre>
</td>
</tr>
</table>

There C Standard implementation states: **The important interfaces of this tool are:**

* `guard` prefixes a guarded block
* `defer` prefixes a defer clause
* `break` ends a guarded block and executes all its defer clauses
* `return` unwinds all guarded blocks of the current function and returns to the caller
* `exit` unwinds all defer clauses of all active function calls of the thread and exits normally
* `panic` starts global unwinding of all guarded blocks
* `recover` inside a defer clause stops a panic and provides an error code

**Planned C11** example from [source](https://gitlab.inria.fr/gustedt/defer/-/blob/master/defer4.c?ref_type=heads) - **gitlab**, outlined in [C Standard WG14 meeting](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2542.pdf) - **pdf**

<table>
<tr>
<th>RAII C89</th>
<th>Planned C11</th>
</tr>
<tr>
<td>

<pre><code>
#include "raii.h"

char number[20];
void g_print(void *ptr) {
    int i = raii_value(ptr)->integer;
    printf("Defer in g = %d.\n", i);
}

void g(int i) {
    if (i > 3) {
        puts("Panicking!");
        snprintf(number, 20, "%ld", i);
        _panic(number);
    }
    guard {
      _defer(g_print, &i);
      printf("Printing in g = %d.\n", i);
      g(i + 1);
    } guarded;
}

void f_print(void *na) {
    puts("In defer in f");
    fflush(stdout);
    if (_recover(_get_message())) {
        printf("Recovered in f = %s\n", _get_message());
        fflush(stdout);
    }
}

void f()
guard {
    _defer(f_print, NULL);
    puts("Calling g.");
    g(0);
    puts("Returned normally from g.");
} guarded;

int main(int argc, char **argv) {
    f();
    puts("Returned normally from f.");
    return EXIT_SUCCESS;
}
</code></pre>

</td>
<td>

<pre><code>
#include "stdio.h"
#include "stddef.h"
#include "threads.h"
#include "stddefer.h"

void g(int i) {
  if (i > 3) {
    puts("Panicking!");
    panic(i);
  }
  guard {
    defer {
      printf("Defer in g = %d.\n", i);
    }
    printf("Printing in g = %d.\n", i);
    g(i+1);
  }
}

void f() {
  guard {
    defer {
        puts("In defer in f");
        fflush(stdout);
      int err = recover();
      if (err != 0) {
        printf("Recovered in f = %d\n", err);
        fflush(stdout);
      }
    }
    puts("Calling g.");
    g(0);
    puts("Returned normally from g.");
  }
}

int main(int argc, char* argv[static argc+1]) {
  f();
  puts("Returned normally from f.");
  return EXIT_SUCCESS;
}
</code></pre>
</td>
</tr>
</table>

_Function_ `​f`​ containing a **​defer​** statement which contains a call to the **​recover**
function. _Function_ `​f`​ invokes _function_ `​g`​ which recursively descends before _panicking_ when the
value of `​i > 3`​. Execution of `​f`​ produces the following **output**:
<pre>
Calling g.
Printing in g = 0.
Printing in g = 1.
Printing in g = 2.
Printing in g = 3.
Panicking!
Defer in g = 3.
Defer in g = 2.
Defer in g = 1.
Defer in g = 0.
In defer in f
Recovered in f = 4
Returned normally from f.
</pre>

## Synopsis

**C++** has a concept called [unique_ptr)](https://en.cppreference.com/w/cpp/memory/unique_ptr) where **"a smart pointer that owns and manages another object through a pointer and disposes of that object when the unique_ptr goes out of scope. "**

Here too the same process is in effect through an **new** _typedef_ `unique_t` aka `memory_t` _structure_.

```c
/* Calls fn (with args as arguments) in separate thread, returning without waiting
for the execution of fn to complete. The value returned by fn can be accessed
by calling `thrd_get()`. */
C_API future thrd_async(thrd_func_t fn, size_t num_of_args, ...);

/* Same as `thrd_async`, allows passing custom `context` scope for internal `promise`
for auto cleanup within caller's `scope`. */
C_API future thrd_async_ex(memory_t *scope, thrd_func_t fn, void_t args);

/* Returns the value of `future` ~promise~, a thread's shared object, If not ready, this
function blocks the calling thread and waits until it is ready. */
C_API values_type thrd_get(future);

/* This function blocks the calling thread and waits until `future` is ready,
will execute provided `yield` callback function continuously. */
C_API void thrd_wait(future, wait_func yield);

/* Check status of `future` object state, if `true` indicates thread execution has ended,
any call thereafter to `thrd_get` is guaranteed non-blocking. */
C_API bool thrd_is_done(future);
C_API void thrd_delete(future);
C_API uintptr_t thrd_self(void);
C_API size_t thrd_cpu_count(void);

/* Return/create an arbitrary `vector/array` set of `values`, only available within `thread/future` */
C_API vectors_t thrd_data(size_t, ...);

/* Return/create an single `vector/array` ~value~, only available within `thread/future` */
#define $(val) thrd_data(1, (val))

/* Return/create an pair `vector/array` ~values~, only available within `thread/future` */
#define $$(val1, val2) thrd_data(2, (val1), (val2))

C_API future_t thrd_scope(void);
C_API future_t thrd_sync(future_t);
C_API rid_t thrd_spawn(thrd_func_t fn, size_t num_of_args, ...);
C_API values_type thrd_result(rid_t id);

// C_API future_t thrd_for(for_func_t loop, intptr_t initial, intptr_t times);

C_API void thrd_then(result_func_t callback, future_t iter, void_t result);
C_API void thrd_destroy(future_t);
C_API bool thrd_is_finish(future_t);

/**
* Creates an scoped `vector/array/container` for arbitrary arguments passing
* into an single `parameter` function.
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `args_destructor_set()` to have memory auto released
*   within ~callers~ scoped `context`, will happen either at return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* NOTE: `vector_for` does auto memory cleanup.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API args_t args_for(size_t, ...);
C_API args_t args_ex(size_t, va_list);
C_API void args_destructor_set(args_t);
C_API bool is_args(void_t);

/**
* Creates an scoped `vector/array/container` for arbitrary item types.
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `array_deferred_set` to have memory auto released
*   when given `scope` context return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API arrays_t array_of(memory_t *, size_t, ...);
C_API void array_deferred_set(arrays_t, memory_t *);
C_API void array_append(arrays_t, void_t);
C_API void array_delete(arrays_t);
C_API void array_remove(arrays_t, int);
C_API bool is_array(void_t);
#define $append(arr, value) array_append((arrays_t)arr, (void_t)value)
#define $remove(arr, index) array_remove((arrays_t)arr, index)

#define vectorize(vec) vectors_t vec = vector_variant()
#define vector(vec, count, ...) vectors_t vec = vector_for(nil, count, __VA_ARGS__)

#define $push_back(vec, value) vector_push_back((vectors_t)vec, (void_t)value)
#define $insert(vec, index, value) vector_insert((vectors_t)vec, index, (void_t)value)
#define $clear(vec) vector_clear((vectors_t)vec)
#define $size(vec) vector_size((vectors_t)vec)
#define $capacity(vec) vector_capacity((vectors_t)vec)
#define $erase(vec, index) vector_erase((vectors_t)vec, index)

/* The `foreach(item in vector/array)` macro, similar to `C#`,
executes a statement or a block of statements for each element in
an instance of `vectors_t/args_t/arrays_t` */
#define foreach(...) foreach_xp(foreach_in, (__VA_ARGS__))
#define foreach_back(...) foreach_xp(foreach_in_back, (__VA_ARGS__))
```

### There is _1 way_ to create an smart memory pointer

```c
/* Creates smart memory pointer, this object binds any additional requests to it's lifetime.
for use with `malloc_*` `calloc_*` wrapper functions to request/return raw memory. */
C_API unique_t *unique_init(void);
```

> This system use macros `RAII_MALLOC`, `RAII_FREE`, `RAII_REALLOC`, and `RAII_CALLOC`.
> If not **defined**, the default **malloc/calloc/realloc/free** are used when expanded.
> The expansion thereto points to custom replacement allocation routines **rpmalloc**.

### The following _malloc/calloc_ wrapper functions are used to get an raw memory pointer

```c
/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void *malloc_full(memory_t *scope, size_t size, func_t func);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void *calloc_full(memory_t *scope, int count, size_t size, func_t func);
```

Note the above functions will **panic/throw** if request fails, is `NULL`,
and begin **unwinding**, executing _deferred_ statements.

### Thereafter, an smart pointer can be use with these _raii_* functions

```c
/* Defer execution `LIFO` of given function with argument,
to the given `scoped smart pointer` lifetime/destruction. */
C_API size_t raii_deferred(memory_t *, func_t, void *);

/* Same as `raii_deferred` but allows recover from an Error condition throw/panic,
you must call `raii_is_caught` inside function to mark Error condition handled. */
C_API void raii_recover_by(memory_t *, func_t, void *);

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
C_API bool raii_is_caught(memory_t *scope, const char *err);

/* Get scoped error condition string. */
C_API const char *raii_message_by(memory_t *scope);

/* Begin `unwinding`, executing given scope smart pointer `raii_deferred` statements. */
C_API void raii_deferred_free(memory_t *);

/* Same as `raii_deferred_free`, but also destroy smart pointer. */
C_API void raii_delete(memory_t *ptr);
```

### Using `thread local storage` for an default smart pointer, the following functions always available

```c
/* Return current `context` ~scope~ smart pointer, in use! */
C_API memory_t *get_scope(void);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `thread` smart pointer, unless called
between `guard` blocks, or inside ~c++11~ like `thread/future` call. */
C_API void *malloc_local(size_t size);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `thread` smart pointer, unless called
between `guard` blocks, or inside ~c++11~ like `thread/future` call. */
C_API void *calloc_local(int count, size_t size);

/* Defer execution `LIFO` of given function with argument,
to current `thread` scope lifetime/destruction. */
C_API size_t raii_defer(func_t, void *);

/* Same as `raii_defer` but allows recover from an Error condition throw/panic,
you must call `raii_caught` inside function to mark Error condition handled. */
C_API void raii_recover(func_t, void *);

/* Compare `err` to current error condition, will mark exception handled, if `true`. */
C_API bool raii_caught(const char *err);

/* Get current error condition string. */
C_API const char *raii_message(void);

/* Begin `unwinding`, executing current `thread` scope `raii_defer` statements. */
C_API void raii_deferred_clean(void);
```

### Fully automatic memory safety, using `guard/unguarded/guarded` macro

The full potently of **RAII** is encapsulated in the `guard` macro.
Using `try/catch/catch_any/catch_if/finally/end_try` exception system macros separately will be unnecessary, however see [examples](https://github.com/zelang-dev/c-raii/tree/main/examples) folder for usage.

```c++
try {
    throw(division_by_zero);
    printf("never reached\n");
} catch_if {
    if (caught(bad_alloc))
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
    else if (caught(division_by_zero))
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);

    ex_backtrace(err_backtrace);
} finally {
    if (err)
        printf("finally: try failed -> %s (%s:%d)\n", err, err_file, err_line);
    else
        printf("finally: try succeeded\n");
} end_try;
```

The Planned C11 implementation details still holds here, but `defer` not confined to `guard` block, actual function call.

```c
/* Creates an scoped guard section, it replaces `{`.
Usage of: `_defer`, `_malloc`, `_calloc`, `_assign_ptr` macros
are only valid between these sections.
    - Use `_return(x);` macro, or `break;` to exit early.
    - Use `_assign_ptr(var)` macro, to make assignment of block scoped smart pointer. */
#define guard

/* This ends an scoped guard section, it replaces `}`.
On exit will begin executing deferred functions,
return given `result` when done, use `NONE` for no return. */
#define unguarded(result)

/* This ends an scoped guard section, it replaces `}`.
On exit will begin executing deferred functions. */
#define guarded

#define _malloc(size) malloc_local(size)
#define _calloc(count, size) calloc_local(count, size)

/* Defer execution `LIFO` of given function with argument,
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call.

Execution begins when current `guard` scope exits or panic/throw. */
C_API size_t deferring(func_t func, void_t data);
#define _defer(func, ptr) deferring(func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`.
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call. */
C_API bool is_recovered(const char *err);
#define _recover(err) is_recovered(err)

/* Get scoped error condition string.
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call. */
C_API const char *err_message(void);
#define _get_message() err_message()

/* Stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
#define _panic(err)

/* Makes a reference assignment of current scoped smart pointer. */
#define _assign_ptr(scope)

/* Exit `guarded` section, begin executing deferred functions,
return given `value` when done, use `NONE` for no return. */
#define _return(value)
```

The idea way of using this library, is to make a **new** _field_ for `unique_t` into your current _typedef_ **object**,
mainly one held throughout application, and setup your wrapper functions to above **raii_** functions.

There are also `2 global callback` functions that need to be setup for complete integration.

```c
// Currently an wrapper function that set ctx->data, scoped error, and protection state, working on removing need
typedef void (*ex_setup_func)(ex_context_t *ctx, const char *ex, const char *panic);
// Your wrapper to raii_deferred_free(ctx->data)
typedef void (*ex_unwind_func)(void *);

ex_setup_func exception_setup_func;
ex_unwind_func exception_unwind_func;
```

## Installation

The build system uses **cmake**, by default produces **static** library stored under `built`, and the complete `include` folder is needed.

**As cmake project dependency**

```c
if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D USE_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer -Wno-return-type")
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D USE_DEBUG")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions("/wd4244 /wd4267 /wd4033 /wd4715")
endif()

add_subdirectory(deps/raii)
target_link_libraries(project PUBLIC raii)
```

**Linux**

```shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug/Release -D BUILD_TESTS=OFF -D BUILD_EXAMPLES=OFF # use to not build tests and examples
cmake --build .
```

**Windows**

```shell
mkdir build
cd build
cmake .. -D BUILD_TESTS=OFF -D BUILD_EXAMPLES=OFF # use to not build tests and examples
cmake --build . --config Debug/Release
```

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/zelang-dev/c-raii/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.
