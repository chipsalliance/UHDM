#ifndef UHDM_DB_HPP
#define UHDM_DB_HPP

#include <memory>
#include <string>
#include <vector>

// Forward declarations for UHDM types
namespace uhdm {
class Serializer;
}
using vpiHandle = uint32_t*;

namespace uhdm_py {

/**
 * @brief A thin C++ wrapper class that owns UHDM serializer state.
 * 
 * This class manages the lifetime of a UHDM Serializer and provides
 * load/save functionality for UHDM binary files.
 */
class UHDMDatabase {
public:
    UHDMDatabase();
    ~UHDMDatabase();

    // Non-copyable, but movable
    UHDMDatabase(const UHDMDatabase&) = delete;
    UHDMDatabase& operator=(const UHDMDatabase&) = delete;
    UHDMDatabase(UHDMDatabase&&) noexcept;
    UHDMDatabase& operator=(UHDMDatabase&&) noexcept;

    /**
     * @brief Load a UHDM binary file from disk.
     * 
     * @param path Path to the UHDM file to load
     * @throws FileError if the file does not exist or cannot be opened
     * @throws SerializationError if the file cannot be parsed
     */
    void load(const std::string& path);

    /**
     * @brief Save the current UHDM state to a binary file.
     * 
     * @param path Path where the UHDM file should be saved
     * @throws FileError if the file cannot be created or written
     * @throws SerializationError if serialization fails
     */
    void save(const std::string& path) const;

    /**
     * @brief Check if a design has been loaded.
     * @return true if designs are loaded, false otherwise
     */
    bool isLoaded() const noexcept;

private:
    std::unique_ptr<uhdm::Serializer> serializer_;
    std::vector<vpiHandle> designs_;
};

}  // namespace uhdm_py

#endif  // UHDM_DB_HPP
