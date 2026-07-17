# ApicManager

A Linux-first, offline CLI tool to organize personal photo libraries by date, using EXIF metadata. Built as a hands-on Modern C++ learning project.

## Why

Photo libraries scattered across folders make it hard to find memories. ApicManager solves this by automatically sorting photos into a clean `Year/Month` structure — entirely offline, with zero risk to your original files.

## Features

- 🔍 **Recursive scanning** — finds all images in nested folders
- 📅 **EXIF date reading** — extracts the actual date a photo was taken
- 🔄 **Smart fallback** — uses file modified date when EXIF data is missing
- 📁 **Automatic organization** — sorts photos into `Year/Month` folders
- 🛡️ **Copy Mode (zero data loss)** — originals are never modified or deleted
- 📊 **Progress bar** — visual feedback during large operations
- 📝 **Logging** — every run generates a detailed audit log

## Tech Stack

- Modern C++20
- `std::filesystem` for file operations
- [`exiv2`](https://exiv2.org/) for EXIF metadata parsing
- CMake / g++ build tools
- Developed on Ubuntu 24.04 LTS (WSL2)

## Usage

```bash
./apicmanager organize <path_to_photo_folder>
```

This creates a new folder named `<path>_organized` alongside your source folder, with photos sorted like:


Your original photos are never touched — only copies are created.

## Project Status

🚧 **Version 1 — Core features complete and tested on real photo data.**

### Roadmap
- [ ] Duplicate detection
- [ ] Search functionality
- [ ] Library statistics
- [ ] Backup/restore
- [ ] GPS-based organization
- [ ] Cross-platform (native Windows support)

## Development Philosophy

This project follows a "learn as you build" approach — each feature was implemented only after understanding the underlying C++ concept, with no code merged that wasn't understood first.

## License

Personal project — license TBD.
