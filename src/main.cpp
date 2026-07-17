#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
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

int main() {
    std::string folderPath = "/home/tomcat/projects/apicmanager/test_photos";

    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        if (fs::is_regular_file(entry)) {
            std::string ext = toLowerCase(entry.path().extension().string());
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                std::string date = getPhotoDate(entry.path().string());

                std::cout << "File: " << entry.path().filename().string();
                if (!date.empty()) {
                    std::cout << " | Date: " << date;
                } else {
                    std::cout << " | Date: NOT FOUND";
                }
                std::cout << std::endl;
            }
        }
    }

    return 0;
}
