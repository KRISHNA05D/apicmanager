#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

std::string toLowerCase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return text;
}

int main() {
    std::string folderPath = "/home/tomcat/projects/apicmanager/test_photos";

    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        if (fs::is_regular_file(entry)) {
            std::string ext = toLowerCase(entry.path().extension().string());
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                std::cout << entry.path() << std::endl;
            }
        }
    }

    return 0;
}
