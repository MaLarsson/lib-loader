# Lib Loader

Sometimes it might be useful to be able to load and execute a static library.
If you, for example, are developing a programming language which can execute arbitrary code during compilation.
In this case you would have to be able to execute any static library function at either runtime or compile time.

The following snippet is an example of how to load and execute functions from a static library.

```c++
#include "lib_loader.h"
#include <stdio.h>

int main() {
    LibLoader lib = {};
    lib_load_file(&lib, "test.lib");

    char *(*greeting)() = (char *(*)())lib_lookup_symbol(&lib, "greeting");
    printf("%s\n", greeting());

    return 0;
}
```
