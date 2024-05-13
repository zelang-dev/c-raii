# c-raii
An robust high-level Defer, RAII implementation for C89, auto smart memory safety.

This library has been decoupled from [c-coroutine](https://github.com/zelang-dev/c-coroutine) to be independently developed.

In the effort to find uniform naming of terms, various other packages was discovered [Except](https://github.com/meaning-matters/Except), [exceptions-and-raii-in-c](https://github.com/psevon/exceptions-and-raii-in-c). Choose to settle in on [A defer mechanism for C](https://gustedt.wordpress.com/2020/12/14/a-defer-mechanism-for-c/), an upcoming C standard compiler feature. It's exactly this library's working design and concepts addressed in [c-coroutine](https://github.com/zelang-dev/c-coroutine). The behavior here is as in **Go** and **Zig** _languages_, simple syntax.

The planned implementation from [defer reference implementation for C](https://gustedt.gitlabpages.inria.fr/defer/):

<table>
<tr>
<th>RAII C89</th>
<th>Planned C11</th>
</tr>
<tr>
<td>

```c
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
```
</td>
<td>

```c
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
```
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

There example from [source](https://gitlab.inria.fr/gustedt/defer/-/blob/master/defer4.c?ref_type=heads) - **gitlab**, outlined in [C Standard WG14 meeting](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2542.pdf) - **pdf**

<table>
<tr>
<th>RAII C89</th>
<th>Planned C11</th>
</tr>
<tr>
<td>

```c
#include "raii.h"

void g_print(void *ptr) {
    int i = raii_value(ptr)->integer;
    printf("Defer in g = %d.\n", i);
}

void g(int i) {
    char number[20];
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
```

</td>
<td>

```c
#include <stdio.h>
#include <stddef.h>
#include <threads.h>
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
```
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
