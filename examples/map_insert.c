/*
Todo: Converted from https://cplusplus.com/reference/unordered_map/unordered_map/insert/
*/

#include "map.h"

int main(int argc, char **argv) {
    map_t myrecipe = map_for(2, kv_double("milk", 2.0), kv_double("flour", 1.5));

    //std::pair<std::string, double> myshopping("baking powder", 0.3);
    map_insert(myrecipe, kv_double("baking powder", 0.3));  // copy insertion
    map_insert(myrecipe, kv_double("eggs", 6.0));           // move insertion

    //myrecipe.insert(mypantry.begin(), mypantry.end());    // range insertion
    map_insert(myrecipe, kv_double("sugar", 0.8));          // initializer list insertion
    map_insert(myrecipe, kv_double("salt", 0.1));           // initializer list insertion

    printf("myrecipe contains:\n");
    foreach_map(x in myrecipe)
        printf("%s : %.3f\n", indic(x), has(x).precision);
    puts("");

    map_free(myrecipe);
    return 0;
}
