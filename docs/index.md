# c-raii

[![windows & linux & macos](https://github.com/zelang-dev/c-raii/actions/workflows/ci.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci.yml)
[![centos](https://github.com/zelang-dev/c-raii/actions/workflows/ci_centos.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_centos.yml)
[![various cpu's](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu.yml)

An robust high-level **Defer**, _RAII_ implementation for `C89`, automatic memory safety, _smarty!_

* [Synopsis](#synopsis)
    - [There is _1 ways_ to create an smart memory pointer.](#there-is-1-way-to-create-an-smart-memory-pointer)
    - [The following _malloc/calloc_ wrapper functions are used to get an raw memory pointer.](#the-following-malloccalloc-wrapper-functions-are-used-to-get-an-raw-memory-pointer)
    - [Thereafter, an smart pointer can be use with these _raii_* functions](#thereafter-an-smart-pointer-can-be-use-with-these-raii-functions)
    - [Using `thread local storage` for an default smart pointer, the following functions always available.](#using-thread-local-storage-for-an-default-smart-pointer-the-following-functions-always-available)
    - [Fully automatic memory safety, using `guard/unguarded/guarded` macro.](#fully-automatic-memory-safety-using-guardunguardedguarded-macro)
* [Installation](#installation)
* [Contributing](#contributing)
* [License](#license)

This library has been decoupled from [c-coroutine](https://zelang-dev.github.io/c-coroutine) to be independently developed.

In the effort to find uniform naming of terms, various other packages was discovered [Except](https://github.com/meaning-matters/Except), and [exceptions-and-raii-in-c](https://github.com/psevon/exceptions-and-raii-in-c). Choose to settle in on [A defer mechanism for C](https://gustedt.wordpress.com/2020/12/14/a-defer-mechanism-for-c/), an upcoming C standard compiler feature. It's exactly this library's working design and concepts addressed in [c-coroutine](https://github.com/zelang-dev/c-coroutine).

This library uses an custom version of [rpmalloc](https://github.com/mjansson/rpmalloc) for **malloc/heap** allocation, not **C11** compiler `thread local` dependant, nor **C++** focus, _removed_. The customization is merged with required [cthread](https://github.com/zelang-dev/cthread) for **C11** _thread like emulation_.

- The behavior here is as in other _languages_ **Go's** [defer](https://go.dev/ref/spec#Defer_statements), **Zig's** [defer](https://ziglang.org/documentation/master/#defer), **Swift's C** [defer](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/statements/#Defer-Statement), even **Rust** has [multi defer crates](https://crates.io/keywords/defer) there are other **borrow checker** issues - [A defer discussion](https://internals.rust-lang.org/t/a-defer-discussion/20387).

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

- `guard` prefixes a guarded block
- `defer` prefixes a defer clause
- `break` ends a guarded block and executes all its defer clauses
- `return` unwinds all guarded blocks of the current function and returns to the caller
- `exit` unwinds all defer clauses of all active function calls of the thread and exits normally
- `panic` starts global unwinding of all guarded blocks
- `recover` inside a defer clause stops a panic and provides an error code

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

### There is _1 way_ to create an smart memory pointer.

```c
/* Creates smart memory pointer, this object binds any additional requests to it's lifetime.
for use with `malloc_*` `calloc_*` wrapper functions to request/return raw memory. */
C_API unique_t *unique_init(void);
```

> This system use macros `RAII_MALLOC`, `RAII_FREE`, `RAII_REALLOC`, and `RAII_CALLOC`.
> If not **defined**, the default __malloc/calloc/realloc/free__ are used when expanded.
> The expansion thereto points to custom replacement allocation routines **rpmalloc**.

### The following _malloc/calloc_ wrapper functions are used to get an raw memory pointer.

```c
/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be `RAII_FREE` when scope smart pointer panics/returns/exits. */
C_API void *malloc_by(memory_t *scope, size_t size);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be `RAII_FREE` when scope smart pointer panics/returns/exits. */
C_API void *calloc_by(memory_t *scope, int count, size_t size);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void *malloc_full(memory_t *scope, size_t size, func_t func);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void *calloc_full(memory_t *scope, int count, size_t size, func_t func);
```

Note the above functions will **panic/throw** if request fails, is `NULL`,
and begin **unwinding**, executing _deferred_ statements.

### Thereafter, an smart pointer can be use with these _raii_* functions.

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

### Using `thread local storage` for an default smart pointer, the following functions always available.

```c
/* Request/return raw memory of given `size`,
uses current `thread` smart pointer,
DO NOT `free`, will be `RAII_FREE`
when `raii_deferred_clean` is called. */
C_API void *malloc_default(size_t size);

/* Request/return raw memory of given `size`,
uses current `thread` smart pointer,
DO NOT `free`, will be `RAII_FREE`
when `raii_deferred_clean` is called. */
C_API void *calloc_default(int count, size_t size);

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

### Fully automatic memory safety, using `guard/unguarded/guarded` macro.

The full potently of **RAII** is encapsulated in the `guard` macro.
Using `try/catch/catch_any/catch_if/finally/end_try` exception system macros separately will be unnecessary, however see [examples](https://github.com/zelang-dev/c-raii/tree/main/examples) folder for usage.

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

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _malloc(size)

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _calloc(count, size)

/* Defer execution `LIFO` of given function with argument,
execution begins when current `guard` scope exits or panic/throw. */
#define _defer(func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
#define _recover(err)

/* Compare `err` to scoped error condition,
will mark exception handled, if `true`.
DO NOT PUT `err` in quote's like "err". */
#define _is_caught(err)

/* Get scoped error condition string. */
#define _get_message()

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
