#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <exiv2/exiv2.hpp>

namespace fs = std::filesystem;

std::string toLowerCase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return text;
}

std::string getPhotoDate(const std::string& imagePath) {
    try {
        auto image = Exiv2::ImageFactory::open(imagePath);
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();

        if (exifData.empty()) {
            return "";
        }

        auto it = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
        if (it != exifData.end()) {
            return it->value().toString();
        }
    } catch (...) {
        return "";
    }
    return "";
}

std::string getFallbackDate(const fs::path& filePath) {
    auto ftime = fs::last_write_time(filePath);

    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

    std::tm* timeInfo = std::localtime(&cftime);

    char buffer[8];
    std::strftime(buffer, sizeof(buffer), "%Y:%m", timeInfo);

    return std::string(buffer);
}

int main() {
    std::string sourceFolder = "/home/tomcat/projects/apicmanager/test_photos";
    std::string destFolder = "/home/tomcat/projects/apicmanager/organized";

    int copiedCount = 0;
    int fallbackCount = 0;

    for (const auto& entry : fs::recursive_directory_iterator(sourceFolder)) {
        if (fs::is_regular_file(entry)) {
            std::string ext = toLowerCase(entry.path().extension().string());
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {

                std::string date = getPhotoDate(entry.path().string());
                std::string year, month;

                if (!date.empty()) {
                    year = date.substr(0, 4);
                    month = date.substr(5, 2);
                } else {
                    std::string fallback = getFallbackDate(entry.path());
                    year = fallback.substr(0, 4);
                    month = fallback.substr(5, 2);
                    fallbackCount++;
                    std::cout << "FALLBACK (using modified date): " << entry.path().filename().string() << std::endl;
                }

                std::string targetDir = destFolder + "/" + year + "/" + month;
                fs::create_directories(targetDir);

                std::string targetPath = targetDir + "/" + entry.path().filename().string();

                fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing);

                std::cout << "COPIED: " << entry.path().filename().string() << " -> " << year << "/" << month << std::endl;
                copiedCount++;
            }
        }
    }

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Copied: " << copiedCount << std::endl;
    std::cout << "Used fallback date: " << fallbackCount << std::endl;

    return 0;
}
