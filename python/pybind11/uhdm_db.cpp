#include "uhdm_db.hpp"
#include "utils/exceptions.hpp"

#include <uhdm/Serializer.h>

#include <filesystem>

namespace uhdm_py {

UHDMDatabase::UHDMDatabase() 
    : serializer_(std::make_unique<uhdm::Serializer>()) {}

UHDMDatabase::~UHDMDatabase() = default;

UHDMDatabase::UHDMDatabase(UHDMDatabase&&) noexcept = default;
UHDMDatabase& UHDMDatabase::operator=(UHDMDatabase&&) noexcept = default;

void UHDMDatabase::load(const std::string& path) {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        throw FileError("File not found: " + path);
    }
    
    // Check if file is readable
    if (!std::filesystem::is_regular_file(path)) {
        throw FileError("Path is not a regular file: " + path);
    }

    // Attempt to restore the UHDM database
    try {
        designs_ = serializer_->restore(path);
    } catch (const std::exception& e) {
        throw SerializationError(std::string("Failed to load UHDM file: ") + e.what());
    }

    // Check if restore returned valid designs
    if (designs_.empty()) {
        throw SerializationError("Failed to load UHDM file: no designs found or version mismatch");
    }
}

void UHDMDatabase::save(const std::string& path) const {
    // Check if parent directory exists
    std::filesystem::path filepath(path);
    auto parent = filepath.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        throw FileError("Parent directory does not exist: " + parent.string());
    }

    // Attempt to save the UHDM database
    try {
        // Note: save() is not const in UHDM Serializer, but logically
        // saving doesn't modify the design content. We use const_cast here.
        const_cast<uhdm::Serializer*>(serializer_.get())->save(path);
    } catch (const std::exception& e) {
        throw SerializationError(std::string("Failed to save UHDM file: ") + e.what());
    }
}

bool UHDMDatabase::isLoaded() const noexcept {
    return !designs_.empty();
}

}  // namespace uhdm_py
