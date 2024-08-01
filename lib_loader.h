#pragma once

#include "array.h"
#include <stdint.h>

struct CoffFile {
    char *file_content;
    uint8_t **section_mapping;
    uint8_t *runtime_base;
    uint8_t *jumptable_base;
};

bool coff_load_file(CoffFile *file, const char *path);
uint8_t *coff_lookup_symbol(CoffFile *file, const char *name);
void coff_free(CoffFile *file);

struct LibLoader {
    Array<CoffFile> coff_files;
    uint8_t *runtime_base;
};
