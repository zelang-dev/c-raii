#include "raii.h"
/* Plain C89 version of https://cplusplus.com/reference/future/future/wait/

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
*/

// a non-optimized way of checking for prime numbers:
void *is_prime(args_t arg) {
    int i, x = get_arg(arg).integer;
    for (i = 2; i < x; ++i) if (x % i == 0) return thrd_value(false);
    return thrd_value(true);
}

int main(int argc, char **argv) {
    int prime = 194232491;
    // call function asynchronously:
    future *fut = thrd_for(is_prime, &prime);

    // a status check, use to guarantee any `thrd_get` call will be ready (and not block)
    // must be part of some external event loop handling routine
    if (!thrd_is_done(fut))
        printf("checking...\n");

    printf("\n194232491 ");
    if (thrd_get(fut).boolean) // blocks and wait for is_prime to return
        printf("is prime.\n");
    else
        printf("is not prime.\n");

    return 0;
}
