#include "lib_loader.h"
#include <stdio.h>

extern "C" int hello_world();

int main() {
    CoffFile coff = {};
    coff_load_file(&coff, "tests/test.obj");

    printf("real hello: %d\n", hello_world());
    int (*hello2)() = (int (*)())coff_lookup_symbol(&coff, "hello_world");
    if (hello2) {
        printf("hello: %d\n", hello2());
    }

    int (*world)() = (int (*)())coff_lookup_symbol(&coff, "world");
    if (world) {
        printf("world: %d\n", world());
    }

    const char *(*greeting1)() = (const char *(*)())coff_lookup_symbol(&coff, "greeting");
    if (greeting1) {
        printf("greeting: %s\n", greeting1());
    }

    int (*value)() = (int (*)())coff_lookup_symbol(&coff, "value");
    if (value) {
        printf("value: %d\n", value());
    }

    void (*set_value)(int) = (void (*)(int))coff_lookup_symbol(&coff, "set_value");
    if (value) {
        set_value(100);
        printf("value: %d\n", value());
    }

    coff_free(&coff);

    LibLoader lib = {};
    lib_load_file(&lib, "tests/test.lib");

    const char *(*greeting)() = (const char *(*)())lib_lookup_symbol(&lib, "greeting");
    if (greeting) {
        printf("greeting: %s\n", greeting());
    }

    lib_free(&lib);

    return 0;
}
