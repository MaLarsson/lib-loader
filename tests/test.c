#include <stdio.h>

int test();

int call_extern() {
    printf("hello!!\n");
    return test();
}

static int i = 10;

int hello_world() {
    return 15;
}

int world() {
    return hello_world() + 5;
}

const char *greeting() {
    return "hello world!";
}

int value() {
    return i;
}

void set_value(int new) {
    i = new;
}
