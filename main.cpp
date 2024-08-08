#include "lib_loader.h"
#include <stdio.h>
#include <dyncall.h>

#include "Windows.h"

int exec_command_line(const char *format, ...) {
    int command_cap = 64 << 20;
    char *command_line = (char *)malloc(command_cap);

    va_list va;
    va_start(va, format);
    vsprintf(command_line, format, va);
    va_end(va);

    STARTUPINFO start_info = {};
    start_info.cb = sizeof(STARTUPINFO);
    start_info.dwFlags = STARTF_USESTDHANDLES;
    start_info.hStdInput = INVALID_HANDLE_VALUE;
    start_info.hStdOutput = INVALID_HANDLE_VALUE;
    start_info.hStdError = INVALID_HANDLE_VALUE;

    int exit_code;
    PROCESS_INFORMATION process_info = {};

    if (CreateProcess(NULL, command_line, NULL, NULL, true, 0, NULL, NULL, &start_info, &process_info)) {
        WaitForSingleObject(process_info.hProcess, INFINITE);
        GetExitCodeProcess(process_info.hProcess, (DWORD *)&exit_code);
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
    } else {
        printf("Failed to execute command:\n\t%s\n", command_line);
        exit_code = -1;
    }

    free(command_line);

    return exit_code;
}

int main() {
    LibLoader lib = {};
    //lib_load_file(&lib, "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.32.31326/lib/x64/libcmt.lib");
    lib_load_file(&lib, "tests/test.lib");

    uint8_t *greeting = lib_lookup_symbol(&lib, "greeting");
    if (greeting) {
        DCCallVM *vm = dcNewCallVM(4096);
        dcMode(vm, DC_CALL_C_DEFAULT);
        dcReset(vm);

        //dcArgDouble(vm, 4.2373);
        const char *dyncall = (const char *)dcCallPointer(vm, (DCpointer)greeting);
        printf("dyncall 'greeting': %s\n", dyncall);

        dcFree(vm);
    }

    lib_free(&lib);

    {
        exec_command_line("link.exe %s /MACHINE:X64 /DLL %s /NOIMPLIB /NOEXP /NOENTRY /OUT:%s",
                          "tests/test.lib", "/EXPORT:greeting", "temp.dll");

        HMODULE handle = LoadLibrary("temp.dll");
        FARPROC dyn_proc = GetProcAddress(handle, "greeting");

        DCCallVM *vm = dcNewCallVM(4096);
        dcMode(vm, DC_CALL_C_DEFAULT);
        dcReset(vm);

        const char *dyncall = (const char *)dcCallPointer(vm, (DCpointer)dyn_proc);
        printf("dll dyncall 'greeting': %s\n", dyncall);

        dcFree(vm);
    }

    return 0;
}
