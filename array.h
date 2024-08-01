#pragma once

#include <stdint.h>

template <typename T>
struct Array {
    T* data;
    uint32_t count;
    uint32_t capacity;
};
