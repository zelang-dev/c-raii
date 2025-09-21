# c-raii

[![Windows & Ubuntu & Apple macOS](https://github.com/zelang-dev/c-raii/actions/workflows/ci.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci.yml)
[![CentOS Stream 9](https://github.com/zelang-dev/c-raii/actions/workflows/ci_centos.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_centos.yml)
[![armv7, aarch64, riscv64](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu.yml)
[![ppc64le - ucontext](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu-ppc64le.yml/badge.svg)](https://github.com/zelang-dev/c-raii/actions/workflows/ci_cpu-ppc64le.yml)

An robust high-level **Defer**, _RAII_ implementation for `C89`, automatic memory safety _smartly_, with **ultra** simple `threading` capabilities.

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

The features now encompass most things you might find in higher level languages, specificity **ease of use**, _with the same old_ **C89**. Not new concepts, you'll find most implicit in every _program/application_, if not explicit, now available in additional header files:

* dynamic data-structures [vector.h](https://github.com/zelang-dev/c-raii/blob/main/include/vector.h), [hashtable.h](https://github.com/zelang-dev/c-raii/blob/main/include/hashtable.h) and [map.h](https://github.com/zelang-dev/c-raii/blob/main/include/map.h).
* parsers [url_http.h](https://github.com/zelang-dev/c-raii/blob/main/include/url_http.h) and [json.h](https://github.com/zelang-dev/c-raii/blob/main/include/json.h).
* execution context and control flow constructs, _threads, coroutines, and channels_, [future.h](https://github.com/zelang-dev/c-raii/blob/main/include/future.h), [coro.h](https://github.com/zelang-dev/c-raii/blob/main/include/coro.h) and [channel.h](https://github.com/zelang-dev/c-raii/blob/main/include/channel.h).
* string manipulation/handling [swar.h](https://github.com/zelang-dev/c-raii/blob/main/include/swar.h).
* access/manipulate an individual integer bits, with string representation [bitset.h](https://github.com/zelang-dev/c-raii/blob/main/include/bitset.h).
* runtime code [reflection.h](https://github.com/zelang-dev/c-raii/blob/main/include/reflection.h), behavior similar to [C++ Reflection - Back on Track](https://youtu.be/nBUgjFPkoto) **video**.

> See [tests](https://github.com/zelang-dev/c-raii/blob/main/tests) and [examples](https://github.com/zelang-dev/c-raii/blob/main/examples) **folder** for _usage_.

This library has come full circle from it's initial aim at being _decoupled_ from ~c-coroutine~ [c-asio](https://zelang-dev.github.io/c-asio) to _bridging_ it back with different startup strategy behavior. Must either **`#define USE_CORO`** or call **`coro_start(main_func, argc, argv, 0)`**. When active **C** become whatever _high level_ language you can imagine that offers _automatic memory management_, otherwise no coroutine support, program will **crash** calling any specific coroutine only functions.

> The one _essential_ thing that all languages must _control_ and _redefine_ doing the compiler creation process, _function_ **"creation/signature/entrance/exit"** _process_. The whole **"signature/entrance/exit"** process hereforth is under **C-RAII** control.

The threading model, _execution context_ provided here is nothing new, the concept has been around for **C** aka _C89_ for quite awhile. The behavior is the same, but called under different _terminology_:

* [Effect handlers](https://en.wikipedia.org/wiki/Effect_system) model with various implementations listed in [effect-handlers-bench](https://github.com/effect-handlers/effect-handlers-bench).
* Various high level languages have direct compiler support for [Async/Await](https://en.wikipedia.org/wiki/Async/await),
but don't have [work stealing](https://en.wikipedia.org/wiki/Work_stealing) in that _paradigm_.

The model here mimics [Go concurrency](https://en.wikipedia.org/wiki/Go_(programming_language)#Concurrency) aka [Green thread](https://en.wikipedia.org/wiki/Green_thread) in _execution_ with follows [Cilk](https://en.wikipedia.org/wiki/Cilk) behavior. There are also [Weave](https://github.com/mratsim/weave) and [Lace](https://github.com/trolando/lace) both compete with **Cilk** behavior. **Weave** using [Nim programming language](https://nim-lang.org/), in which it's origins is `C`, and still compiles down to. **Lace** is `C11` base. In fact **Go** initial origins started as `C`, it's just a _matter of time_ before a [turing-complete](https://en.wikipedia.org/wiki/Turing_completeness) _understanding_ take effect, build _self_ with _self_. **It's amazing how things always come full circle**.

**_Everything that follows at this point is from version `1.x`, noteing ~c-coroutine~ [c-asio repo](https://zelang-dev.github.io/c-asio) will be restructured/refactored for the sole purpose of integrating [libuv](https://github.com/libuv/libuv), the coroutine part to be removed._**

---

This library has been decoupled from ~c-coroutine~ [c-asio](https://zelang-dev.github.io/c-asio) to be independently developed.

In the effort to find uniform naming of terms, various other packages was discovered [Except](https://github.com/meaning-matters/Except), and [exceptions-and-raii-in-c](https://github.com/psevon/exceptions-and-raii-in-c). Choose to settle in on [A defer mechanism for C](https://gustedt.wordpress.com/2020/12/14/a-defer-mechanism-for-c/), an upcoming C standard compiler feature. It's exactly this library's working design and concepts addressed in [c-asio](https://github.com/zelang-dev/c-asio).

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
    if (_recovered(_get_message())) {
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
This uses current `context` smart pointer, being in `guard` blocks,
inside `thread/future`, or active `coroutine` call. */
C_API void *malloc_local(size_t size);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `context` smart pointer, being in `guard` blocks,
inside `thread/future`, or active `coroutine` call. */
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
Using `try/catch/catch_any/catch_if/finally/finality` exception system macros separately will be unnecessary, however see [examples](https://github.com/zelang-dev/c-raii/tree/main/examples) folder for usage.

Below is the **recommended pattern** for complete cross platform usage.
System uses _native_ [Windows SEH](https://learn.microsoft.com/en-us/cpp/cpp/try-except-statement), _multiple_ `catch()` blocks not possible.

> NOTE: Only in **debug builds** _uncaught exceptions_ **backtrace** info is auto displayed.

```c++
#include "raii.h"

static void pfree(void *p) {
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int main(int argc, char **argv) {
    try {
        int a = 1;
        int b = 0.00000;
        char *p = 0;
        p = malloc_full(raii_init(), 3, pfree);
        strcpy(p, "p");

        *(int *)0 = 0;
        printf("never reached\n");
        printf("%d\n", (a / b));
        raise(SIGINT);
    } catch_if {
        if (caught(bad_alloc))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(division_by_zero))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_fpe))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_ill))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_int))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
        else if (caught(sig_segv))
            printf("catch: exception %s (%s:%d) caught\n", err.name, err.file, err.line);
    } finally {
        if (err.is_caught) {
            printf("finally: try failed, but succeeded to catch -> %s (%s:%d)\n", err.name, err.file, err.line);
        } else {
            printf("finally: try failed to `catch()`\n");
            ex_backtrace(err.backtrace);
        }
    }

    return 0;
}
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
Only valid between `guard` blocks or inside `thread/future` call.

Execution begins when current `guard` scope exits or panic/throw. */
#define _defer(func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`.
Only valid between `guard` blocks or inside `thread/future` call. */
#define _recovered(err)

/* Get scoped error condition string.
Only valid between `guard` blocks or inside `thread/future` call. */
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

Any **commit** with an **tag** is considered _stable_ for **release** at that _version_ point.

If there are no _binary_ available for your platform under **Releases** then build using **cmake**,
which produces **static** libraries by default.

You will need the _binary_ stored under `built`, and `*.h` headers, or complete `include` _folder_ if **Windows**.

### Linux

```shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug/Release -D BUILD_TESTS=OFF -D BUILD_EXAMPLES=OFF # use to not build tests and examples
cmake --build .
```

### Windows

```shell
mkdir build
cd build
cmake .. -D BUILD_TESTS=OFF -D BUILD_EXAMPLES=OFF # use to not build tests and examples
cmake --build . --config Debug/Release
```

### As cmake project dependency

> For **CMake** versions earlier than `3.14`, see <https://cmake.org/cmake/help/v3.14/module/FetchContent.html>

Add to **CMakeLists.txt**

```c
find_package(raii QUIET)
if(NOT raii_FOUND)
    FetchContent_Declare(raii
        URL https://github.com/zelang-dev/c-raii/archive/refs/tags/2.1.3.zip
        URL_MD5 147623b14f65c8201547225708ced4e3
    )
    FetchContent_MakeAvailable(raii)
endif()

target_include_directories(your_project PUBLIC $<BUILD_INTERFACE:${RAII_INCLUDE_DIR} $<INSTALL_INTERFACE:${RAII_INCLUDE_DIR})
target_link_libraries(your_project PUBLIC raii)
```

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/zelang-dev/c-raii/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.
