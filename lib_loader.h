#pragma once

#include "slice.h"
#include <stdint.h>

struct CoffFile {
    char *content_base;
    uint8_t **section_mapping;
    uint8_t *runtime_base;
    uint8_t *jumptable_base;
};

struct LibLoader {
    char *file_content;
    Slice<CoffFile> coff_files;
    Slice<char *> symbol_table;
    uint16_t *offset_table;
    uint8_t *runtime_base;
};

bool lib_load_file(LibLoader *lib, const char *path);
uint8_t *lib_lookup_symbol(LibLoader *lib, const char *name);
void lib_free(LibLoader *lib);
