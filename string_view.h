#pragma once

#include <string.h>

struct StringView {
    char *data;
    uint32_t length;
};

static inline bool string_compare(const StringView &lhs, const StringView &rhs) {
    if (lhs.length != rhs.length) {
        return false;
    }

    for (uint32_t i = 0; i < lhs.length; i += 1) {
        if (lhs.data[i] != rhs.data[i]) {
            return false;
        }
    }

    return true;
}

static inline StringView string_view(const char *text, uint32_t length) { return { (char *)text, length }; }
static inline StringView string_view(const char *text) { return string_view(text, strlen(text)); }

static inline bool string_compare(const StringView &lhs, const char *rhs) {
    return string_compare(lhs, string_view(rhs));
}

static inline bool string_compare(const char *lhs, const StringView &rhs) {
    return string_compare(string_view(lhs), rhs);
}

static inline bool string_compare(const char *lhs, const char *rhs) {
    return string_compare(string_view(lhs), string_view(rhs));
}
