#include "lib_loader.h"
#include <stdio.h>

#include "Windows.h"

int main() {
    LibLoader lib = {};
    //lib_load_file(&lib, "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.32.31326/lib/x64/libcmt.lib");
    lib_load_file(&lib, "tests/test.lib");

    const char *(*greeting)() = (const char *(*)())lib_lookup_symbol(&lib, "greeting");
    if (greeting) {
        printf("greeting: %s\n", greeting());
    }

    int (*call_extern)() = (int (*)())lib_lookup_symbol(&lib, "call_extern");
    if (call_extern) {
        //printf("call_extern: %d\n", call_extern());
    }

    lib_free(&lib);

    return 0;
}
