#pragma once
#include <string>

namespace seed {

class CrashHandler {
public:
    static void install();
    static void triggerAssert(const char* condition, const char* message, const char* file, int line);
    static std::string getStackTrace();
    static void generateMinidump();

private:
    static void onSignal(int sig);
};

} // namespace seed
