#include "SystemUtils.hpp"
#include <cstdlib>
#include <thread>
#include <iostream>

namespace Utils {
    namespace SystemUtils {

        void openFileExplorer(const std::string& path) {
            std::string command;

#ifdef _WIN32
            command = "explorer.exe \"" + path + "\"";
#elif __APPLE__
            command = "open \"" + path + "\"";
#elif __linux__
            command = "xdg-open \"" + path + "\"";
#else
            std::cerr << "Unsupported platform for openFileExplorer" << std::endl;
            return;
#endif

            // Launch the system command in a detached thread to avoid blocking the UI
            std::thread([command]() {
                int ret = std::system(command.c_str());
                if (ret != 0) {
                    std::cerr << "Failed to open file explorer with command: " << command << std::endl;
                }
            }).detach();
        }

    }
}
