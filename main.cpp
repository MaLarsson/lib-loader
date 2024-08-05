#include "lib_loader.h"
#include <stdio.h>

int main() {
    LibLoader lib = {};
    lib_load_file(&lib, "tests/test.lib");

    const char *(*greeting)() = (const char *(*)())lib_lookup_symbol(&lib, "greeting");
    if (greeting) {
        printf("greeting: %s\n", greeting());
    }

    /*
    int (*call_extern)() = (int (*)())lib_lookup_symbol(&lib, "call_extern");
    if (call_extern) {
        printf("call_extern: %d\n", call_extern());
    }
    */

    lib_free(&lib);

    return 0;
}
