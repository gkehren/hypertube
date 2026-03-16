#include "SystemUtils.hpp"
#include <iostream>
#include <string>
#include <cassert>

void testSanitizePath() {
    // Note: This test runs in a POSIX environment, so it will test the POSIX logic
    // We cannot easily test the Windows logic here without changing the environment.

    std::string testPath = "-help";
    std::string sanitized = Utils::SystemUtils::sanitizePath(testPath);

    #ifdef _WIN32
    assert(sanitized == ".\\-help");
    #else
    assert(sanitized == "./-help");
    #endif

    assert(Utils::SystemUtils::sanitizePath("normal.txt") == "normal.txt");
    assert(Utils::SystemUtils::sanitizePath("") == "");

    std::cout << "Path sanitization tests passed!" << std::endl;
}

int main() {
    testSanitizePath();
    return 0;
}
