#include "lib_loader.h"
#include "slice.h"
#include "string_view.h"

#include <stdlib.h>
#include <stdio.h>

#include "Windows.h"

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("Could not open file \"%s\".\n", path);
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(sizeof(char) * (file_size + 1));
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    fclose(file);

    if (bytes_read < file_size) {
        free(buffer);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    return buffer;
}

#pragma pack(push, 1)
struct CoffSymbol {
    union {
        struct { uint32_t is_short, offset; };
        char short_name[8];
    };
    uint32_t value;
    int16_t section_number;
    uint8_t complex_type;
    uint8_t base_type;
    uint8_t storage_class;
    uint8_t number_of_aux_symbols;
};

struct CoffRelocation {
    uint32_t virtual_address;
    uint32_t symbol_table_index;
    uint16_t type;
};

struct ExternalJump {
    uint8_t *address;
    uint8_t instruction[6];
};
#pragma pack(pop)

static void init_jump_instruction(ExternalJump *ext_jump, uint8_t *address) {
    ext_jump->address = address;

    // x64 unconditional JMP with address stored at -14 bytes offset.
    ext_jump->instruction[0] = 0xff;
    ext_jump->instruction[1] = 0x25;
    ext_jump->instruction[2] = 0xf2;
    ext_jump->instruction[3] = 0xff;
    ext_jump->instruction[4] = 0xff;
    ext_jump->instruction[5] = 0xff;
}

static Slice<CoffSymbol> coff_symbol_table(CoffFile *file) {
    IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)file->content_base;
    CoffSymbol *symbol_base = (CoffSymbol *)(file->content_base + coff_header->PointerToSymbolTable);
    return { symbol_base, coff_header->NumberOfSymbols };
}

static char *coff_string_table(CoffFile *file) {
    IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)file->content_base;
    CoffSymbol *symbol_base = (CoffSymbol *)(file->content_base + coff_header->PointerToSymbolTable);
    return (char *)(symbol_base + coff_header->NumberOfSymbols);
}

static uint32_t page_align(uint32_t page_size, uint32_t size) {
    return (size + (page_size - 1)) & ~(page_size - 1);
}

static StringView section_name(CoffFile *file, IMAGE_SECTION_HEADER *header) {
    char *name = (char *)&header->Name;
    if (name[0] == '/') {
        // TODO handle long section names.
    }
    // TODO remove $xxx suffix from section names?
    return string_view(name, name[7] == '\0' ? strlen(name) : 8);
}

static StringView symbol_name(CoffFile *file, CoffSymbol *symbol) {
    if (symbol->is_short) {
        char *name = symbol->short_name;
        return string_view(name, name[7] == '\0' ? strlen(name) : 8);
    }
    return string_view(&coff_string_table(file)[symbol->offset]);
}

static uint32_t protection_constant(CoffFile *file, IMAGE_SECTION_HEADER *section) {
    bool read  = section->Characteristics & IMAGE_SCN_MEM_READ;
    bool write = section->Characteristics & IMAGE_SCN_MEM_WRITE;
    bool exec  = section->Characteristics & IMAGE_SCN_MEM_EXECUTE;

    if (read && write && exec) {
        return PAGE_EXECUTE_READWRITE;
    }
    if (read && exec) {
        return PAGE_EXECUTE_READ;
    }
    if (exec) {
        return PAGE_EXECUTE;
    }
    if (read && write) {
        return PAGE_READWRITE;
    }
    if (read) {
        return PAGE_READONLY;
    }

    StringView name = section_name(file, section);
    printf("ERROR: unable to select protection constant for section '%.*s'.\n", name.length, name.data);

    return PAGE_NOACCESS;
}

static bool coff_load_file(CoffFile *file, char *content) {
    file->content_base = content;

    IMAGE_FILE_HEADER *coff_header = (IMAGE_FILE_HEADER *)file->content_base;
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    Slice<IMAGE_SECTION_HEADER> section_headers;
    section_headers.data = (IMAGE_SECTION_HEADER *)(file->content_base + sizeof(IMAGE_FILE_HEADER));
    section_headers.count = coff_header->NumberOfSections;

    // Collect the total size of the runtime data.
    uint32_t runtime_data_size = 0;
    for (IMAGE_SECTION_HEADER &section : section_headers) {
        bool loadable = (section.Characteristics & IMAGE_SCN_MEM_EXECUTE ||
                         section.Characteristics & IMAGE_SCN_MEM_READ ||
                         section.Characteristics & IMAGE_SCN_MEM_WRITE);
        bool discardable = (section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE);

        if (loadable && !discardable) {
            runtime_data_size += page_align(system_info.dwPageSize, section.SizeOfRawData);
        }
    }

    file->section_mapping = (uint8_t **)calloc(section_headers.count, sizeof(uint8_t *));
    file->runtime_base = (uint8_t *)VirtualAlloc(NULL, runtime_data_size, MEM_COMMIT, PAGE_READWRITE);
    uint8_t *cursor = file->runtime_base;

    // Copy the data over from the sections into the allocated runtime sections.
    int section_index = 0;
    for (IMAGE_SECTION_HEADER &section : section_headers) {
        bool loadable = (section.Characteristics & IMAGE_SCN_MEM_EXECUTE ||
                         section.Characteristics & IMAGE_SCN_MEM_READ ||
                         section.Characteristics & IMAGE_SCN_MEM_WRITE);
        bool discardable = (section.Characteristics & IMAGE_SCN_MEM_DISCARDABLE);

        if (loadable && !discardable) {
            memcpy(cursor, file->content_base + section.PointerToRawData, section.SizeOfRawData);
            file->section_mapping[section_index] = cursor;
            cursor += page_align(system_info.dwPageSize, section.SizeOfRawData);
        }

        section_index += 1;
    }

    Slice<CoffSymbol> symbol_table = coff_symbol_table(file);

    // Resolve any relocations for the sections copied into the runtime.
    section_index = 0;
    for (IMAGE_SECTION_HEADER &section : section_headers) {
        uint8_t *section_runtime_base = file->section_mapping[section_index];

        if (section_runtime_base) {
            CoffRelocation *relocations_base = (CoffRelocation *)(file->content_base + section.PointerToRelocations);
            uint32_t number_of_relocations = section.NumberOfRelocations;

            for (CoffRelocation &reloc : make_slice(relocations_base, number_of_relocations)) {
                CoffSymbol *symbol = symbol_table[reloc.symbol_table_index];
                uint8_t *symbol_runtime_base = file->section_mapping[symbol->section_number - 1];

                if (symbol_runtime_base) {
                    uint8_t *patch_offset = section_runtime_base + reloc.virtual_address;
                    uint8_t *symbol_address = symbol_runtime_base + symbol->value;

                    switch (reloc.type) {
                    case IMAGE_REL_AMD64_ADDR32NB:
                        // TODO handle this relocation type.
                        break;
                    case IMAGE_REL_AMD64_REL32:
                        *((uint32_t *)patch_offset) = symbol_address - 4 - patch_offset;
                        break;
                    default:
                        printf("ERROR: unhandled relocation type %d\n", reloc.type);
                        break;
                    }
                }
            }
        }

        section_index += 1;
    }

    // Set protection levels for the sections copied into the runtime.
    section_index = 0;
    for (IMAGE_SECTION_HEADER &section : section_headers) {
        uint8_t *section_runtime_base = file->section_mapping[section_index];

        if (section_runtime_base) {
            uint32_t aligned_section_size = page_align(system_info.dwPageSize, section.SizeOfRawData);
            DWORD old_status;
            VirtualProtect(section_runtime_base, aligned_section_size, protection_constant(file, &section), &old_status);
        }

        section_index += 1;
    }

    return true;
}

static uint8_t *coff_lookup_symbol(CoffFile *file, const char *name) {
    Slice<CoffSymbol> symbol_table = coff_symbol_table(file);

    for (uint32_t i = 0; i < symbol_table.count; i += 1) {
        CoffSymbol *symbol = symbol_table[i];

        if (string_compare(symbol_name(file, symbol), name)) {
            return file->runtime_base + symbol->value;
        }

        i += symbol->number_of_aux_symbols;
    }

    return NULL;
}

static void coff_free(CoffFile *file) {
    VirtualFree(file->runtime_base, 0, MEM_RELEASE);
    free(file->section_mapping);
}

struct LibArchiveHeader {
    char name[16];
    char date[12];
    char user_id[6];
    char group_id[6];
    char mode[8];
    char size[10];
    char end_of_header[2];
};

struct LibParseContext {
    LibLoader *lib;
    char *cursor;
};

static inline bool is_digit(char c) {
    return c > '0' && c < '9';
}

static uint32_t parse_archive_size(char *cursor) {
    uint32_t result = 0;

    while (is_digit(*cursor)) {
        result *= 10;
        result += *cursor - '0';
        cursor += 1;
    }

    return result;
}

bool lib_load_file(LibLoader *lib, const char *path) {
    lib->file_content = read_file(path);
    if (!lib->file_content) {
        printf("ERROR: unable to read lib file.\n");
        return false;
    }

    char *cursor = lib->file_content;
    StringView signature = string_view(lib->file_content, IMAGE_ARCHIVE_START_SIZE);
    cursor += IMAGE_ARCHIVE_START_SIZE;

    if (!string_compare(signature, IMAGE_ARCHIVE_START)) {
        printf("ERROR: incorrect signature in lib file.\n");
        free(lib->file_content);
        return false;
    }

    // Skip first linker member.
    LibArchiveHeader *first = (LibArchiveHeader *)cursor;
    cursor += sizeof(LibArchiveHeader);

    if (!string_compare(string_view(first->name, 16), IMAGE_ARCHIVE_LINKER_MEMBER)) {
        printf("ERROR: no first linker member in lib file.\n");
        free(lib->file_content);
        return false;
    }

    cursor += parse_archive_size(first->size);
    if (*cursor == '\n') {
        cursor += 1;
    }

    // Parse second linker member.
    LibArchiveHeader *second = (LibArchiveHeader *)cursor;
    cursor += sizeof(LibArchiveHeader);

    if (!string_compare(string_view(second->name, 16), IMAGE_ARCHIVE_LINKER_MEMBER)) {
        printf("ERROR: no second linker member in lib file.\n");
        free(lib->file_content);
        return false;
    }

    uint32_t number_of_archives = *(uint32_t *)cursor;
    lib->coff_files.data = (CoffFile *)calloc(number_of_archives, sizeof(CoffFile));
    lib->coff_files.count = number_of_archives;

    // Parse the symbol table.
    cursor += sizeof(uint32_t) + sizeof(uint32_t) * number_of_archives;
    uint32_t number_of_symbols = *(uint32_t *)cursor;
    cursor += sizeof(uint32_t);

    lib->symbol_table.data = (char **)malloc(sizeof(char *) * number_of_symbols);
    lib->symbol_table.count = number_of_symbols;
    lib->offset_table = (uint16_t *)cursor;
    cursor += sizeof(uint16_t) * number_of_symbols;

    for (uint32_t i = 0; i < number_of_symbols; i += 1) {
        lib->symbol_table.data[i] = cursor;
        cursor += strlen(cursor) + 1;
    }

    // Reset the cursor to the beginning of next member.
    cursor = (char *)second + parse_archive_size(second->size) + sizeof(LibArchiveHeader);
    if (*cursor == '\n') {
        cursor += 1;
    }

    // TODO: check for IMAGE_ARCHIVE_LONGNAMES_MEMBER.

    for (uint32_t i = 0; i < number_of_archives; i += 1) {
        LibArchiveHeader *header = (LibArchiveHeader *)cursor;
        cursor += sizeof(LibArchiveHeader);

        coff_load_file(&lib->coff_files.data[i], cursor);

        cursor += parse_archive_size(header->size);
        if (*cursor == '\n') {
            cursor += 1;
        }
    }

    return true;
}

void lib_free(LibLoader *lib) {
    for (CoffFile &coff : lib->coff_files) {
        coff_free(&coff);
    }
    free(lib->symbol_table.data);
    free(lib->file_content);
}

uint8_t *lib_lookup_symbol(LibLoader *lib, const char *name) {
    uint32_t i = 0;
    for (char *symbol : lib->symbol_table) {
        if (string_compare(symbol, name)) {
            uint16_t offset = lib->offset_table[i];
            return coff_lookup_symbol(lib->coff_files[offset - 1], name);
        }
        i += 1;
    }
    return NULL;
}
