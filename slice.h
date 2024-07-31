#pragma once

#include <stdint.h>

template <typename T>
struct Slice {
    T *data;
    uint32_t count;

    T *operator[](uint32_t index) { return &data[index]; }
};

template <typename T> inline Slice<T> make_slice(T *data, uint32_t count) { return { data, count }; }
template <typename T> inline T *begin(Slice<T> &array) { return array.data; }
template <typename T> inline T *end(Slice<T> &array) { return array.data + array.count; }
