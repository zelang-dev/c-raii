#include "reflection.h"

/* converted from https://www.geeksforgeeks.org/reflection-in-golang/ */

// Example program to show difference between
// Type and Kind and to demonstrate use of
// Methods provided by Go reflect Package

reflect_struct(details,
               (STRING, string, fname),
               (STRING, string, lname),
               (INTEGER, int, age),
               (DOUBLE, double, balance)
)

typedef string myType;

int showDetails(reflect_type_t *i, interface j) {
    char value[20] = {0};
    int index;
    reflect_field_t *val = reflect_value_of(i);

    string_t t1 = reflect_type_of(i);
    string_t k1 = reflect_kind(i);

    string_t t2 = reflect_type_of(j);
    string_t k2 = reflect_kind(j);

    printf("Type of first interface: %s\n", t1);
    printf("Kind of first interface: %s\n\n", k1);
    printf("Type of second interface: %s\n", t2);
    printf("Kind of second interface: %s\n", k2);

    printf("\nThe values in the first argument are :\n");
    if (strcmp(reflect_kind(i), "struct") == 0) {
        int numberOfFields = (int)reflect_num_fields(i);
        for (index = 0; index < numberOfFields; index++) {
            reflect_get_field(i, index, value);
            if (index == 3 || index == 0)
                printf("%d.Type: %s || Value: %d\n",
                       (index + 1), reflect_field_type(i, index), c_int(value));
            else if (index == 4)
                printf("%d.Type: %s || Value: %.3f\n",
                       (index + 1), reflect_field_type(i, index), c_double(value));
            else
                printf("%d.Type: %s || Value: %s\n",
                       (index + 1), reflect_field_type(i, index), c_char_ptr(value));

            printf("Kind is: %s\n", reflect_kind(val + index));
        }
    }

    reflect_get_field(j, 1, value);
    printf("\nThe Value passed in second parameter is: %s\n", c_char_ptr(value));

    return exit_scope();
}

int main(int argc, char **argv) {
    as_string_ref(iD, myType, "12345678");

    as_instance_ref(person, details);
    person->fname = "Go";
    person->lname = "Geek";
    person->age = 32;
    person->balance = 33000.203;

    return showDetails(person_r, iD_r);
}
