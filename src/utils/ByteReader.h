#ifndef BYTEREADER_H
#define BYTEREADER_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>

namespace VBDecompiler {

/**
 * @brief Safe binary data reader with bounds checking
 * 
 * Provides safe reading of various data types from a byte buffer
 * with automatic bounds checking and offset management.
 */
class ByteReader {
public:
    /**
     * @brief Construct a ByteReader from a byte span
     * @param data The byte data to read from
     */
    explicit ByteReader(std::span<const std::byte> data)
        : data_(data), offset_(0) {}

    /**
     * @brief Get the current read offset
     */
    [[nodiscard]] size_t offset() const { return offset_; }

    /**
     * @brief Get the total size of the data
     */
    [[nodiscard]] size_t size() const { return data_.size(); }

    /**
     * @brief Get remaining bytes from current offset
     */
    [[nodiscard]] size_t remaining() const { return data_.size() - offset_; }

    /**
     * @brief Check if we can read n bytes
     */
    [[nodiscard]] bool canRead(size_t n) const {
        return offset_ + n <= data_.size();
    }

    /**
     * @brief Set the read offset
     * @throws std::out_of_range if offset is beyond data size
     */
    void seek(size_t newOffset) {
        if (newOffset > data_.size()) {
            throw std::out_of_range("ByteReader::seek: offset out of range");
        }
        offset_ = newOffset;
    }

    /**
     * @brief Skip n bytes
     * @throws std::out_of_range if skip would go beyond data size
     */
    void skip(size_t n) {
        if (!canRead(n)) {
            throw std::out_of_range("ByteReader::skip: not enough bytes");
        }
        offset_ += n;
    }

    /**
     * @brief Read a value of type T
     * @tparam T The type to read (must be trivially copyable)
     * @throws std::out_of_range if not enough bytes available
     */
    template<typename T>
    [[nodiscard]] T read() {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        
        if (!canRead(sizeof(T))) {
            throw std::out_of_range("ByteReader::read: not enough bytes");
        }
        
        T value;
        std::memcpy(&value, data_.data() + offset_, sizeof(T));
        offset_ += sizeof(T);
        return value;
    }

    /**
     * @brief Read a uint8_t
     */
    [[nodiscard]] uint8_t readUInt8() { return read<uint8_t>(); }

    /**
     * @brief Read a uint16_t (little-endian)
     */
    [[nodiscard]] uint16_t readUInt16() { return read<uint16_t>(); }

    /**
     * @brief Read a uint32_t (little-endian)
     */
    [[nodiscard]] uint32_t readUInt32() { return read<uint32_t>(); }

    /**
     * @brief Read a uint64_t (little-endian)
     */
    [[nodiscard]] uint64_t readUInt64() { return read<uint64_t>(); }

    /**
     * @brief Read a null-terminated string
     * @param maxLength Maximum length to read (0 = no limit)
     * @return The string without null terminator
     */
    [[nodiscard]] std::string readCString(size_t maxLength = 0) {
        std::string result;
        size_t startOffset = offset_;
        
        while (offset_ < data_.size()) {
            auto byte = static_cast<char>(data_[offset_++]);
            
            if (byte == '\0') {
                break;
            }
            
            result += byte;
            
            if (maxLength > 0 && result.length() >= maxLength) {
                break;
            }
        }
        
        return result;
    }

    /**
     * @brief Read a fixed-length string
     * @param length Number of bytes to read
     * @return The string (may contain null bytes)
     */
    [[nodiscard]] std::string readString(size_t length) {
        if (!canRead(length)) {
            throw std::out_of_range("ByteReader::readString: not enough bytes");
        }
        
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += static_cast<char>(data_[offset_++]);
        }
        
        return result;
    }

    /**
     * @brief Read raw bytes
     * @param length Number of bytes to read
     * @return Vector of bytes
     */
    [[nodiscard]] std::vector<std::byte> readBytes(size_t length) {
        if (!canRead(length)) {
            throw std::out_of_range("ByteReader::readBytes: not enough bytes");
        }
        
        std::vector<std::byte> result(data_.begin() + offset_, 
                                       data_.begin() + offset_ + length);
        offset_ += length;
        return result;
    }

    /**
     * @brief Get a span of bytes without advancing offset
     * @param length Number of bytes to peek
     * @return Span of bytes at current offset
     */
    [[nodiscard]] std::span<const std::byte> peek(size_t length) const {
        if (!canRead(length)) {
            throw std::out_of_range("ByteReader::peek: not enough bytes");
        }
        
        return data_.subspan(offset_, length);
    }

    /**
     * @brief Get a span of bytes at a specific offset without changing current offset
     * @param offset The offset to read from
     * @param length Number of bytes to peek
     * @return Span of bytes at specified offset
     */
    [[nodiscard]] std::span<const std::byte> peekAt(size_t offset, size_t length) const {
        if (offset + length > data_.size()) {
            throw std::out_of_range("ByteReader::peekAt: not enough bytes");
        }
        
        return data_.subspan(offset, length);
    }

    /**
     * @brief Read a value at a specific offset without changing current offset
     * @tparam T The type to read
     * @param offset The offset to read from
     * @return The value read
     */
    template<typename T>
    [[nodiscard]] T readAt(size_t offset) const {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        
        if (offset + sizeof(T) > data_.size()) {
            throw std::out_of_range("ByteReader::readAt: not enough bytes");
        }
        
        T value;
        std::memcpy(&value, data_.data() + offset, sizeof(T));
        return value;
    }

private:
    std::span<const std::byte> data_;
    size_t offset_;
};

} // namespace VBDecompiler

#endif // BYTEREADER_H
