#include "lib_loader.h"
#include <stdio.h>

extern "C" int hello_world();

int main() {
    CoffFile coff = {};
    coff_load_file(&coff, "tests/test.obj");

    printf("real hello: %d\n", hello_world());
    int (*hello2)() = (int (*)())coff_lookup_symbol(&coff, "hello_world");
    if (hello2) {
        int result = hello2();
        printf("hello: %d\n", result);
    }

    int (*world)() = (int (*)())coff_lookup_symbol(&coff, "world");
    if (world) {
        int result = world();
        printf("world: %d\n", result);
    }

    char *(*greeting)() = (char *(*)())coff_lookup_symbol(&coff, "greeting");
    if (greeting) {
        char *result = greeting();
        printf("greeting: %s\n", result);
    }

    int (*value)() = (int (*)())coff_lookup_symbol(&coff, "value");
    if (value) {
        int result = value();
        printf("value: %d\n", result);
    }

    void (*set_value)(int) = (void (*)(int))coff_lookup_symbol(&coff, "set_value");
    if (value) {
        set_value(100);
        int result = value();
        printf("value: %d\n", result);
    }

    coff_free(&coff);

    return 0;
}
