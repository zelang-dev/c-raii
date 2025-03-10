/*
An converted version of `Golang` example from https://go.dev/tour/moretypes/8

package main

import "fmt"

func main() {
    names := [4]string{
        "John",
        "Paul",
        "George",
        "Ringo",
    }
    fmt.Println(names)

    a := names[0:2]
    b := names[1:3]
    fmt.Println(a, b)

    b[0] = "XXX"
    fmt.Println(a, b)
    fmt.Println(names)
}
*/

#include "reflection.h"

int main(int argc, char **argv) {
    map_array_t names = map_array(of_string, 4, "John", "Paul", "George", "Ringo");
    println(1, names);

    slice_t a = slice(names, 0, 2);
    slice_t b = slice(names, 1, 3);
    println(2, a, b);

    slice_put(b, 0, "XXXX");
    println(2, a, b);

    println(1, names);

    return exit_scope();
}