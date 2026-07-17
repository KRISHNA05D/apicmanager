#include <iostream>
#include <exiv2/exiv2.hpp>

int main() {
    std::string imagePath = "/home/tomcat/projects/apicmanager/test_photos/real/WIN_20250530_09_30_12_Pro.jpg";

    auto image = Exiv2::ImageFactory::open(imagePath);
    image->readMetadata();

    Exiv2::ExifData &exifData = image->exifData();

    if (exifData.empty()) {
        std::cout << "No EXIF data found." << std::endl;
        return 0;
    }

    auto it = exifData.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"));
    if (it != exifData.end()) {
        std::string dateStr = it->value().toString();
        std::cout << "Full Date: " << dateStr << std::endl;

        std::string year = dateStr.substr(0, 4);
        std::string month = dateStr.substr(5, 2);

        std::cout << "Year: " << year << std::endl;
        std::cout << "Month: " << month << std::endl;
    } else {
        std::cout << "DateTimeOriginal tag not found." << std::endl;
    }

    return 0;
}

