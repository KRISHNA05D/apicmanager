#ifndef SHARED_H
#define SHARED_H

#include <filesystem>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <sstream>
#include <exiv2/exiv2.hpp>

namespace fs = std::filesystem;

inline std::string toLowerCase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return text;
}

inline bool isImageFile(const fs::path& path) {
    std::string ext = toLowerCase(path.extension().string());
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
}

inline std::string getPhotoDate(const std::string& imagePath) {
    try {
        auto image = Exiv2::ImageFactory::open(imagePath);
        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();
        if (exifData.empty()) return "";
        auto it = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
        if (it != exifData.end()) return it->value().toString();
    } catch (...) {
        return "";
    }
    return "";
}

inline std::string getFallbackDate(const fs::path& filePath) {
    try {
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm* timeInfo = std::localtime(&cftime);
        char buffer[8];
        std::strftime(buffer, sizeof(buffer), "%Y:%m", timeInfo);
        return std::string(buffer);
    } catch (...) {
        return "1970:01";
    }
}

inline std::string getCurrentTimestamp() {
    auto now = std::time(nullptr);
    std::tm* timeInfo = std::localtime(&now);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", timeInfo);
    return std::string(buffer);
}

inline std::string computeFileHash(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    std::hash<std::string> hasher;
    size_t hashValue = hasher(content);
    std::ostringstream hexStream;
    hexStream << std::hex << hashValue;
    return hexStream.str();
}

#endif
