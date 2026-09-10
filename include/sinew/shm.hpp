#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sinew {

/**
 * @brief Cross-platform RAII Shared Memory Region for Zero-Copy IPC.
 *
 * Supports creating new shared memory regions or opening existing ones
 * on Windows (CreateFileMapping) and POSIX (shm_open + mmap).
 */
class SharedMemoryRegion {
public:
    SharedMemoryRegion() noexcept = default;

    ~SharedMemoryRegion() {
        close();
    }

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;

    SharedMemoryRegion(SharedMemoryRegion&& other) noexcept
        : name_(std::move(other.name_)),
          data_(other.data_),
          size_(other.size_),
          is_creator_(other.is_creator_)
#if defined(_WIN32)
        , handle_(other.handle_)
#else
        , fd_(other.fd_)
#endif
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.is_creator_ = false;
#if defined(_WIN32)
        other.handle_ = nullptr;
#else
        other.fd_ = -1;
#endif
    }

    SharedMemoryRegion& operator=(SharedMemoryRegion&& other) noexcept {
        if (this != &other) {
            close();
            name_ = std::move(other.name_);
            data_ = other.data_;
            size_ = other.size_;
            is_creator_ = other.is_creator_;
#if defined(_WIN32)
            handle_ = other.handle_;
            other.handle_ = nullptr;
#else
            fd_ = other.fd_;
            other.fd_ = -1;
#endif
            other.data_ = nullptr;
            other.size_ = 0;
            other.is_creator_ = false;
        }
        return *this;
    }

    static SharedMemoryRegion create(std::string_view name, size_t size) {
        SharedMemoryRegion region;
        region.name_ = std::string(name);
        region.size_ = size;
        region.is_creator_ = true;

#if defined(_WIN32)
        std::string win_name = "Local\\" + region.name_;
        ULARGE_INTEGER li;
        li.QuadPart = static_cast<ULONGLONG>(size);

        HANDLE hMap = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            li.HighPart,
            li.LowPart,
            win_name.c_str()
        );

        if (hMap == nullptr) {
            return SharedMemoryRegion{};
        }

        void* ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (ptr == nullptr) {
            CloseHandle(hMap);
            return SharedMemoryRegion{};
        }

        region.handle_ = hMap;
        region.data_ = ptr;
#else
        std::string posix_name = (name.empty() || name[0] != '/') ? ("/" + region.name_) : region.name_;
        int fd = shm_open(posix_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0660);
        if (fd < 0) {
            return SharedMemoryRegion{};
        }

        if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
            ::close(fd);
            shm_unlink(posix_name.c_str());
            return SharedMemoryRegion{};
        }

        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd);
            shm_unlink(posix_name.c_str());
            return SharedMemoryRegion{};
        }

        region.fd_ = fd;
        region.data_ = ptr;
#endif
        return region;
    }

    static SharedMemoryRegion open(std::string_view name, size_t size) {
        SharedMemoryRegion region;
        region.name_ = std::string(name);
        region.size_ = size;
        region.is_creator_ = false;

#if defined(_WIN32)
        std::string win_name = "Local\\" + region.name_;
        HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, win_name.c_str());
        if (hMap == nullptr) {
            return SharedMemoryRegion{};
        }

        void* ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (ptr == nullptr) {
            CloseHandle(hMap);
            return SharedMemoryRegion{};
        }

        region.handle_ = hMap;
        region.data_ = ptr;
#else
        std::string posix_name = (name.empty() || name[0] != '/') ? ("/" + region.name_) : region.name_;
        int fd = shm_open(posix_name.c_str(), O_RDWR, 0660);
        if (fd < 0) {
            return SharedMemoryRegion{};
        }

        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd);
            return SharedMemoryRegion{};
        }

        region.fd_ = fd;
        region.data_ = ptr;
#endif
        return region;
    }

    void close() noexcept {
        if (data_ != nullptr) {
#if defined(_WIN32)
            UnmapViewOfFile(data_);
#else
            munmap(data_, size_);
#endif
            data_ = nullptr;
        }

#if defined(_WIN32)
        if (handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
#else
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        if (is_creator_ && !name_.empty()) {
            std::string posix_name = (name_[0] != '/') ? ("/" + name_) : name_;
            shm_unlink(posix_name.c_str());
        }
#endif
        size_ = 0;
        is_creator_ = false;
    }

    void* data() noexcept { return data_; }
    const void* data() const noexcept { return data_; }
    size_t size() const noexcept { return size_; }
    bool is_valid() const noexcept { return data_ != nullptr; }
    explicit operator bool() const noexcept { return is_valid(); }
    const std::string& name() const noexcept { return name_; }

private:
    std::string name_{};
    void* data_{nullptr};
    size_t size_{0};
    bool is_creator_{false};

#if defined(_WIN32)
    HANDLE handle_{nullptr};
#else
    int fd_{-1};
#endif
};

} // namespace sinew
