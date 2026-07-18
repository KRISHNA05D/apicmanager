#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <vector>
#include <fstream>
#include <sstream>
#include <exiv2/exiv2.hpp>
#include <sqlite3.h>

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
        if (exifData.empty()) return "";
        auto it = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
        if (it != exifData.end()) return it->value().toString();
    } catch (...) {
        return "";
    }
    return "";
}

std::string getFallbackDate(const fs::path& filePath) {
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
        return "1970:01"; // ultimate fallback if even modified-date read fails
    }
}

std::string getCurrentTimestamp() {
    auto now = std::time(nullptr);
    std::tm* timeInfo = std::localtime(&now);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", timeInfo);
    return std::string(buffer);
}

std::string computeFileHash(const fs::path& filePath) {
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

void printHelp(const char* programName) {
    std::cout << "ApicManager - Offline photo organizer using EXIF metadata\n" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << programName << " organize <source_folder>\n" << std::endl;
    std::cout << "Description:" << std::endl;
    std::cout << "  Scans <source_folder> recursively for images (.jpg, .jpeg, .png)," << std::endl;
    std::cout << "  reads their EXIF date (falling back to file modified date if missing)," << std::endl;
    std::cout << "  and copies them into <source_folder>_organized/Year/Month/." << std::endl;
    std::cout << "  Original files are never modified or deleted." << std::endl;
    std::cout << "  Duplicate files (same content) are automatically skipped.\n" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help, -h    Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {

    // Handle --help / -h anywhere as the first argument
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printHelp(argv[0]);
        return argc < 2 ? 1 : 0;
    }

    if (argc < 3) {
        std::cout << "Error: Missing source folder.\n" << std::endl;
        printHelp(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    std::string sourceFolder = argv[2];

    if (command != "organize") {
        std::cout << "Error: Unknown command '" << command << "'\n" << std::endl;
        printHelp(argv[0]);
        return 1;
    }

    if (!fs::exists(sourceFolder)) {
        std::cout << "Error: Folder does not exist: " << sourceFolder << std::endl;
        return 1;
    }

    if (!fs::is_directory(sourceFolder)) {
        std::cout << "Error: Path is not a folder: " << sourceFolder << std::endl;
        return 1;
    }

    std::string destFolder = sourceFolder + "_organized";

    sqlite3* db;
    std::string dbPath = "apicmanager.db";
    if (sqlite3_open(dbPath.c_str(), &db)) {
        std::cout << "Error: Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS photos ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "filename TEXT,"
        "filepath TEXT,"
        "year TEXT,"
        "month TEXT,"
        "file_hash TEXT UNIQUE"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cout << "Error creating table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }

    std::string logFileName = "apicmanager_log_" + getCurrentTimestamp() + ".txt";
    std::ofstream logFile(logFileName);
    logFile << "ApicManager Log - " << getCurrentTimestamp() << std::endl;
    logFile << "Source folder: " << sourceFolder << std::endl;
    logFile << "Destination folder: " << destFolder << std::endl;
    logFile << "------------------------------------" << std::endl;

    std::vector<fs::path> imageFiles;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(
                sourceFolder, fs::directory_options::skip_permission_denied)) {
            if (fs::is_regular_file(entry) && isImageFile(entry.path())) {
                imageFiles.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error while scanning folder: " << e.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    int total = imageFiles.size();
    std::cout << "Found " << total << " image files to organize.\n" << std::endl;
    logFile << "Found " << total << " image files to organize." << std::endl;

    int copiedCount = 0;
    int fallbackCount = 0;
    int duplicateCount = 0;
    int errorCount = 0;
    int processed = 0;

    std::string checkSQL = "SELECT id FROM photos WHERE file_hash = ?;";
    std::string insertSQL = "INSERT INTO photos (filename, filepath, year, month, file_hash) VALUES (?, ?, ?, ?, ?);";

    for (const auto& filePath : imageFiles) {
        processed++;

        std::string fileHash;
        try {
            fileHash = computeFileHash(filePath);
        } catch (...) {
            errorCount++;
            logFile << "ERROR (could not read file): " << filePath.filename().string() << std::endl;
            printProgress(processed, total);
            continue;
        }

        sqlite3_stmt* checkStmt;
        sqlite3_prepare_v2(db, checkSQL.c_str(), -1, &checkStmt, nullptr);
        sqlite3_bind_text(checkStmt, 1, fileHash.c_str(), -1, SQLITE_STATIC);
        bool isDuplicate = (sqlite3_step(checkStmt) == SQLITE_ROW);
        sqlite3_finalize(checkStmt);

        if (isDuplicate) {
            duplicateCount++;
            logFile << "DUPLICATE (skipped): " << filePath.filename().string() << std::endl;
            printProgress(processed, total);
            continue;
        }

        std::string date = getPhotoDate(filePath.string());
        std::string year, month;
        bool usedFallback = false;

        if (!date.empty() && date.size() >= 7) {
            year = date.substr(0, 4);
            month = date.substr(5, 2);
        } else {
            std::string fallback = getFallbackDate(filePath);
            year = fallback.substr(0, 4);
            month = fallback.substr(5, 2);
            fallbackCount++;
            usedFallback = true;
        }

        try {
            std::string targetDir = destFolder + "/" + year + "/" + month;
            fs::create_directories(targetDir);
            std::string targetPath = targetDir + "/" + filePath.filename().string();
            fs::copy_file(filePath, targetPath, fs::copy_options::overwrite_existing);

            sqlite3_stmt* insertStmt;
            sqlite3_prepare_v2(db, insertSQL.c_str(), -1, &insertStmt, nullptr);
            sqlite3_bind_text(insertStmt, 1, filePath.filename().string().c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 2, targetPath.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 3, year.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 4, month.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insertStmt, 5, fileHash.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(insertStmt);
            sqlite3_finalize(insertStmt);

            logFile << "COPIED: " << filePath.filename().string()
                    << " -> " << year << "/" << month
                    << (usedFallback ? " [fallback date used]" : " [EXIF date used]")
                    << std::endl;
            copiedCount++;

        } catch (const std::exception& e) {
            errorCount++;
            logFile << "ERROR (copy failed): " << filePath.filename().string()
                    << " - " << e.what() << std::endl;
        }

        printProgress(processed, total);
    }

    sqlite3_close(db);

    logFile << "------------------------------------" << std::endl;
    logFile << "Summary: Copied " << copiedCount << ", Fallback used " << fallbackCount
             << ", Duplicates skipped " << duplicateCount
             << ", Errors " << errorCount << std::endl;
    logFile.close();

    std::cout << "\n\n--- Summary ---" << std::endl;
    std::cout << "Copied: " << copiedCount << std::endl;
    std::cout << "Duplicates skipped: " << duplicateCount << std::endl;
    std::cout << "Used fallback date: " << fallbackCount << std::endl;
    std::cout << "Errors: " << errorCount << std::endl;
    std::cout << "Log saved to: " << logFileName << std::endl;
    std::cout << "Database saved to: " << dbPath << std::endl;

    return 0;
}
