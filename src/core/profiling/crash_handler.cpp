#include "core/profiling/crash_handler.h"
#include "core/profiling/seed_assert.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <execinfo.h>
#include <unistd.h>
#endif

namespace seed {

void CrashHandler::install() {
    std::signal(SIGSEGV, &CrashHandler::onSignal);
    std::signal(SIGABRT, &CrashHandler::onSignal);
    std::signal(SIGFPE, &CrashHandler::onSignal);
#ifdef _WIN32
    SetUnhandledExceptionFilter([](PEXCEPTION_POINTERS) -> LONG {
        generateMinidump();
        return EXCEPTION_EXECUTE_HANDLER;
    });
#endif
}

void CrashHandler::onSignal(int sig) {
    const char* name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV"; break;
        case SIGABRT: name = "SIGABRT"; break;
        case SIGFPE:  name = "SIGFPE";  break;
    }
    std::cerr << "[CRASH] Signal: " << name << "\n";
    std::cerr << getStackTrace() << "\n";
#ifdef _WIN32
    generateMinidump();
#else
    std::signal(SIGABRT, SIG_DFL);
    std::abort();
#endif
    std::_Exit(1);
}

std::string CrashHandler::getStackTrace() {
    std::ostringstream oss;
    oss << "Stack trace:\n";
#ifdef _WIN32
    oss << "  (Windows stack trace requires CaptureStackBackTrace)\n";
#else
    void* buffer[64];
    int nptrs = backtrace(buffer, 64);
    char** strings = backtrace_symbols(buffer, nptrs);
    if (strings) {
        for (int i = 0; i < nptrs; ++i) {
            oss << "  " << strings[i] << "\n";
        }
        free(strings);
    }
#endif
    return oss.str();
}

void CrashHandler::generateMinidump() {
#ifdef _WIN32
    HANDLE hProcess = GetCurrentProcess();
    DWORD pid = GetCurrentProcessId();
    char filename[256];
    snprintf(filename, sizeof(filename), "seed_crash_%lu.dmp", pid);
    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = nullptr;
        mdei.ClientPointers = FALSE;
        MiniDumpWriteDump(hProcess, pid, hFile, MiniDumpNormal, &mdei, nullptr, nullptr);
        CloseHandle(hFile);
    }
#else
    // Linux: rely on core dumps via ulimit -c unlimited
#endif
}

void CrashHandler::triggerAssert(const char* condition, const char* message, const char* file, int line) {
    std::cerr << "[ASSERT FAILED] " << condition << " at " << file << ":" << line << "\n";
    std::cerr << "Message: " << (message ? message : "(none)") << "\n";
    std::cerr << getStackTrace() << "\n";
#ifdef _WIN32
    generateMinidump();
#endif
    std::_Exit(1);
}

} // namespace seed
