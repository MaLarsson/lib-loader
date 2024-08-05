#include "lib_loader.h"
#include <stdio.h>

int main() {
    LibLoader lib = {};
    lib_load_file(&lib, "tests/test.lib");

    const char *(*greeting)() = (const char *(*)())lib_lookup_symbol(&lib, "greeting");
    if (greeting) {
        printf("greeting: %s\n", greeting());
    }

    lib_free(&lib);

    return 0;
}
