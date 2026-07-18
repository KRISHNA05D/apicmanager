#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QTextEdit>
#include <QFileDialog>
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
        return "1970:01";
    }
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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("ApicManager");
    window.resize(500, 300);

    QLabel *instructionLabel = new QLabel("Photo folder path:");
    QLineEdit *pathInput = new QLineEdit();
    QPushButton *browseButton = new QPushButton("Browse...");
    QPushButton *organizeButton = new QPushButton("Organize Photos");
    QProgressBar *progressBar = new QProgressBar();
    QTextEdit *logOutput = new QTextEdit();
    logOutput->setReadOnly(true);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(instructionLabel);
    layout->addWidget(pathInput);
    layout->addWidget(browseButton);
    layout->addWidget(organizeButton);
    layout->addWidget(progressBar);
    layout->addWidget(logOutput);

    window.setLayout(layout);

    // Browse button opens a folder picker
    QObject::connect(browseButton, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&window, "Select Photo Folder");
        if (!dir.isEmpty()) {
            pathInput->setText(dir);
        }
    });

    // Organize button runs the full pipeline
    QObject::connect(organizeButton, &QPushButton::clicked, [&]() {
        std::string sourceFolder = pathInput->text().toStdString();

        if (sourceFolder.empty() || !fs::exists(sourceFolder)) {
            logOutput->append("Error: Please select a valid folder.");
            return;
        }

        std::string destFolder = sourceFolder + "_organized";

        sqlite3* db;
        sqlite3_open("apicmanager.db", &db);
        const char* createTableSQL =
            "CREATE TABLE IF NOT EXISTS photos ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "filename TEXT, filepath TEXT, year TEXT, month TEXT,"
            "file_hash TEXT UNIQUE);";
        sqlite3_exec(db, createTableSQL, nullptr, nullptr, nullptr);

        std::vector<fs::path> imageFiles;
        for (const auto& entry : fs::recursive_directory_iterator(
                sourceFolder, fs::directory_options::skip_permission_denied)) {
            if (fs::is_regular_file(entry) && isImageFile(entry.path())) {
                imageFiles.push_back(entry.path());
            }
        }

        int total = imageFiles.size();
        logOutput->append("Found " + QString::number(total) + " image files.");
        progressBar->setMaximum(total);
        progressBar->setValue(0);

        int copiedCount = 0, fallbackCount = 0, duplicateCount = 0;
        std::string checkSQL = "SELECT id FROM photos WHERE file_hash = ?;";
        std::string insertSQL = "INSERT INTO photos (filename, filepath, year, month, file_hash) VALUES (?, ?, ?, ?, ?);";

        int processed = 0;
        for (const auto& filePath : imageFiles) {
            processed++;
            std::string fileHash = computeFileHash(filePath);

            sqlite3_stmt* checkStmt;
            sqlite3_prepare_v2(db, checkSQL.c_str(), -1, &checkStmt, nullptr);
            sqlite3_bind_text(checkStmt, 1, fileHash.c_str(), -1, SQLITE_STATIC);
            bool isDuplicate = (sqlite3_step(checkStmt) == SQLITE_ROW);
            sqlite3_finalize(checkStmt);

            if (isDuplicate) {
                duplicateCount++;
                progressBar->setValue(processed);
                qApp->processEvents(); // keep GUI responsive
                continue;
            }

            std::string date = getPhotoDate(filePath.string());
            std::string year, month;
            if (!date.empty() && date.size() >= 7) {
                year = date.substr(0, 4);
                month = date.substr(5, 2);
            } else {
                std::string fallback = getFallbackDate(filePath);
                year = fallback.substr(0, 4);
                month = fallback.substr(5, 2);
                fallbackCount++;
            }

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

            copiedCount++;
            progressBar->setValue(processed);
            qApp->processEvents(); // keep GUI responsive during the loop
        }

        sqlite3_close(db);

        logOutput->append("--- Done ---");
        logOutput->append("Copied: " + QString::number(copiedCount));
        logOutput->append("Duplicates skipped: " + QString::number(duplicateCount));
        logOutput->append("Fallback dates used: " + QString::number(fallbackCount));
    });

    window.show();
    return app.exec();
}
