#include "CrashDump.h"

#if defined(_WIN32)
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dbghelp.h>
 #include <stdio.h>
 #include <time.h>

static void makeCrashDir (wchar_t* dir, size_t dirChars)
{
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW (nullptr, exe, MAX_PATH);

    wchar_t* slash = wcsrchr (exe, L'\\');
    if (slash != nullptr)
        *slash = 0;

    _snwprintf_s (dir, dirChars, _TRUNCATE, L"%s\\crashes", exe);
    CreateDirectoryW (dir, nullptr);
}

static void writeDump (EXCEPTION_POINTERS* ep)
{
    wchar_t dir[MAX_PATH] = {};
    makeCrashDir (dir, MAX_PATH);

    time_t now = time (nullptr);
    tm local {};
    localtime_s (&local, &now);

    wchar_t stamp[32] = {};
    wcsftime (stamp, 32, L"%Y%m%d-%H%M%S", &local);

    wchar_t dumpPath[MAX_PATH] = {};
    wchar_t txtPath[MAX_PATH] = {};
    _snwprintf_s (dumpPath, MAX_PATH, _TRUNCATE, L"%s\\crash-%s.dmp", dir, stamp);
    _snwprintf_s (txtPath, MAX_PATH, _TRUNCATE, L"%s\\crash-%s.txt", dir, stamp);

    HANDLE file = CreateFileW (dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei {};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        const auto type = (MINIDUMP_TYPE) (MiniDumpWithIndirectlyReferencedMemory
                                           | MiniDumpWithThreadInfo
                                           | MiniDumpWithUnloadedModules);

        MiniDumpWriteDump (GetCurrentProcess(),
                           GetCurrentProcessId(),
                           file,
                           type,
                           ep != nullptr ? &mei : nullptr,
                           nullptr,
                           nullptr);
        CloseHandle (file);
    }

    HANDLE txt = CreateFileW (txtPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (txt != INVALID_HANDLE_VALUE)
    {
        char buf[512] = {};
        const unsigned int code = (ep != nullptr && ep->ExceptionRecord != nullptr)
                                      ? (unsigned int) ep->ExceptionRecord->ExceptionCode
                                      : 0;
        const void* addr = (ep != nullptr && ep->ExceptionRecord != nullptr)
                               ? ep->ExceptionRecord->ExceptionAddress
                               : nullptr;
        const int n = sprintf_s (buf,
                                 "AI Voice Translator crash\r\n"
                                 "exception=0x%08X\r\n"
                                 "address=%p\r\n"
                                 "dump=%ls\r\n",
                                 code,
                                 addr,
                                 dumpPath);
        if (n > 0)
        {
            DWORD written = 0;
            WriteFile (txt, buf, (DWORD) n, &written, nullptr);
        }
        CloseHandle (txt);
    }

    wchar_t msg[MAX_PATH + 128] = {};
    _snwprintf_s (msg, MAX_PATH + 128, _TRUNCATE,
                  L"AI Voice Translator crashed.\nA dump was written to:\n%s",
                  dumpPath);
    MessageBoxW (nullptr, msg, L"AI Voice Translator", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

static LONG WINAPI sehFilter (EXCEPTION_POINTERS* ep)
{
    writeDump (ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

void installCrashHandler()
{
    SetUnhandledExceptionFilter (sehFilter);
}

void writeCppExceptionReport (const char* what, const char* file, int line)
{
    wchar_t dir[MAX_PATH] = {};
    makeCrashDir (dir, MAX_PATH);

    time_t now = time (nullptr);
    tm local {};
    localtime_s (&local, &now);
    wchar_t stamp[32] = {};
    wcsftime (stamp, 32, L"%Y%m%d-%H%M%S", &local);

    wchar_t txtPath[MAX_PATH] = {};
    _snwprintf_s (txtPath, MAX_PATH, _TRUNCATE, L"%s\\exception-%s.txt", dir, stamp);

    HANDLE txt = CreateFileW (txtPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (txt == INVALID_HANDLE_VALUE)
        return;

    char buf[1024] = {};
    const int n = sprintf_s (buf,
                             "AI Voice Translator C++ exception\r\nwhat=%s\r\nfile=%s\r\nline=%d\r\n",
                             what != nullptr ? what : "(unknown)",
                             file != nullptr ? file : "",
                             line);
    if (n > 0)
    {
        DWORD written = 0;
        WriteFile (txt, buf, (DWORD) n, &written, nullptr);
    }
    CloseHandle (txt);
}

#else

void installCrashHandler() {}
void writeCppExceptionReport (const char*, const char*, int) {}

#endif
