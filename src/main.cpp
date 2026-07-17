#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <vector>
#include <fstream>
#include <exiv2/exiv2.hpp>

namespace fs = std::filesystem;

std::string toLowerCase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return text;
}

bool isImageFile(const fs::path& path) {
    std::string ext = toLowerCase(path.extension().string());
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
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

std::string getCurrentTimestamp() {
    auto now = std::time(nullptr);
    std::tm* timeInfo = std::localtime(&now);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", timeInfo);
    return std::string(buffer);
}

void printProgress(int current, int total) {
    int barWidth = 40;
    float progress = (total == 0) ? 0 : (float)current / total;
    int pos = barWidth * progress;

    std::cout << "\r[";
    for (int i = 0; i < barWidth; i++) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << current << "/" << total << " files   ";
    std::cout.flush();
}

int main(int argc, char* argv[]) {

    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " organize <source_folder>" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    std::string sourceFolder = argv[2];

    if (command != "organize") {
        std::cout << "Unknown command: " << command << std::endl;
        return 1;
    }

    if (!fs::exists(sourceFolder)) {
        std::cout << "Error: Folder does not exist: " << sourceFolder << std::endl;
        return 1;
    }

    std::string destFolder = sourceFolder + "_organized";

    // Set up log file
    std::string logFileName = "apicmanager_log_" + getCurrentTimestamp() + ".txt";
    std::ofstream logFile(logFileName);
    logFile << "ApicManager Log - " << getCurrentTimestamp() << std::endl;
    logFile << "Source folder: " << sourceFolder << std::endl;
    logFile << "Destination folder: " << destFolder << std::endl;
    logFile << "------------------------------------" << std::endl;

    // Pass 1: collect all image files first
    std::vector<fs::path> imageFiles;
    for (const auto& entry : fs::recursive_directory_iterator(sourceFolder)) {
        if (fs::is_regular_file(entry) && isImageFile(entry.path())) {
            imageFiles.push_back(entry.path());
        }
    }

    int total = imageFiles.size();
    std::cout << "Found " << total << " image files to organize.\n" << std::endl;
    logFile << "Found " << total << " image files to organize." << std::endl;

    // Pass 2: process each file with progress bar
    int copiedCount = 0;
    int fallbackCount = 0;

    for (const auto& filePath : imageFiles) {
        std::string date = getPhotoDate(filePath.string());
        std::string year, month;
        bool usedFallback = false;

        if (!date.empty()) {
            year = date.substr(0, 4);
            month = date.substr(5, 2);
        } else {
            std::string fallback = getFallbackDate(filePath);
            year = fallback.substr(0, 4);
            month = fallback.substr(5, 2);
            fallbackCount++;
            usedFallback = true;
        }

        std::string targetDir = destFolder + "/" + year + "/" + month;
        fs::create_directories(targetDir);

        std::string targetPath = targetDir + "/" + filePath.filename().string();
        fs::copy_file(filePath, targetPath, fs::copy_options::overwrite_existing);

        logFile << "COPIED: " << filePath.filename().string()
                << " -> " << year << "/" << month
                << (usedFallback ? " [fallback date used]" : " [EXIF date used]")
                << std::endl;

        copiedCount++;
        printProgress(copiedCount, total);
    }

    logFile << "------------------------------------" << std::endl;
    logFile << "Summary: Copied " << copiedCount << ", Fallback used " << fallbackCount << std::endl;
    logFile.close();

    std::cout << "\n\n--- Summary ---" << std::endl;
    std::cout << "Copied: " << copiedCount << std::endl;
    std::cout << "Used fallback date: " << fallbackCount << std::endl;
    std::cout << "Log saved to: " << logFileName << std::endl;

    return 0;
}
