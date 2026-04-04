//
// Created by asherjil on 4/4/26.
//

#ifndef FPGA_DRIVER_SHMBACKEND_H
#define FPGA_DRIVER_SHMBACKEND_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstdio>
#include <type_traits>
#include <new>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

template<typename T>
concept ShmSlot = std::is_trivially_copyable_v<T> && (alignof(T) & (CACHE_LINE_SIZE - 1)) == 0;

enum class ShmMode {
    Writer,
    Reader
};

template<ShmMode Mode>
class ShmBackend {
public:
    // Implement the Rule of 5
    ShmBackend() = default;
    ShmBackend(const ShmBackend&) = delete;
    ShmBackend(ShmBackend&& other) noexcept
        :m_fd{std::exchange(other.m_fd, -1)},
        m_size{std::exchange(other.m_size, 0)},
        m_baseAddress{std::exchange(other.m_baseAddress, nullptr)},
        m_name{std::move(other.m_name)}{}

    ShmBackend& operator=(const ShmBackend&) = delete;
    ShmBackend& operator=(ShmBackend&& other) noexcept {
        if (this != &other) {
            close();

            m_fd = std::exchange(other.m_fd, -1);
            m_size = std::exchange(other.m_size, 0);
            m_baseAddress = std::exchange(other.m_baseAddress, nullptr);
            m_name = std::move(other.m_name);
        }
        return *this;
    }

    ~ShmBackend() {
        close(); // call close usefull for debugging purposes
    }

    template<ShmSlot T, typename... Members>
    [[nodiscard]] bool open(const char* name, Members T::*... members)
        requires (Mode == ShmMode::Writer)
    {
        if (!openImpl(name, sizeof(T))) {
            return false;
        }
        T* slot = ptr<T>();
        (new (&(slot->*members)) Members(0), ...);
        return true;
        // TODO: Add concise success log about the shared memeory region
    }

    template<ShmSlot T>
    [[nodiscard]] bool open(const char* name)
        requires (Mode == ShmMode::Reader)
    {
        return openImpl(name, sizeof(T));
        // TODO: Add concise success log about the shared memeory region
    }

    void close() {
        if (isOpen()) {
            ::munmap(m_baseAddress, m_size);
        }
        if (m_fd != -1) {
            ::close(m_fd);
        }
        if constexpr (Mode == ShmMode::Writer) {
            if (!m_name.empty()) {
                ::shm_unlink(m_name.c_str());
            }
        }

        m_fd = -1;
        m_baseAddress = nullptr;
        m_size = 0;
        m_name.clear();

        // TODO: add concise logs about successfull closure
    }

    [[nodiscard]] bool isOpen() const noexcept {
        return m_baseAddress != nullptr && m_baseAddress != MAP_FAILED;
    }

    [[nodiscard]] void* getBaseAddress() const noexcept {
        return m_baseAddress;
    }

    [[nodiscard]] std::size_t getMmapSize() const noexcept {
        return m_size;
    }

    // Writer gets T*, Reader gets const T* therefore the return type is auto.
    template <typename T>
    [[nodiscard, gnu::always_inline]]
    inline auto* ptr(std::size_t offset = 0) const noexcept {
        std::uint8_t* base = static_cast<std::uint8_t*>(m_baseAddress) + offset;

        if constexpr (Mode == ShmMode::Writer) {
            return reinterpret_cast<T*>(base);
        }
        else {
            return reinterpret_cast<const T*>(base);
        }
    }
private:
    int           m_fd{-1};
    std::size_t m_size{0};

    void*   m_baseAddress{nullptr};
    std::string m_name{};

    [[nodiscard]] bool openImpl(const char* name, std::size_t size) {
        if (isOpen()) {
            std::fprintf(stderr, "[Error] Unable to open the shared memory because its already open.\n");
            return false;
        }

        if constexpr (Mode == ShmMode::Writer) {
            m_fd = ::shm_open(name, O_CREAT | O_RDWR, 0666);
        }
        else {
            m_fd = ::shm_open(name, O_RDONLY, 0);
        }

        if (m_fd < 0) {
            std::fprintf(stderr, "[Error] Unable to open the shared memory file descriptor.\n");
            return false;
        }

        m_name = name; // save the name inside the string

        if constexpr (Mode == ShmMode::Writer) {
            if (::ftruncate(m_fd, static_cast<off_t>(size)) != 0) {
                std::fprintf(stderr, "[Error] Unable to truncate the shared memeory region.\n");
                ::close(m_fd);
                m_fd = -1;
                m_name.clear(); 
                return false;
            }
        }

        constexpr int prot = (Mode == ShmMode::Writer) ? (PROT_READ | PROT_WRITE) : PROT_READ;
        m_baseAddress = ::mmap(nullptr, size, prot, MAP_SHARED, m_fd, 0);
        if (m_baseAddress == MAP_FAILED) {
            std::fprintf(stderr, "[Error] Unable memory map the shared memory region.\n");
            ::close(m_fd);
            m_fd = -1;
            m_baseAddress = nullptr;
            m_name.clear();
            return false;
        }

        m_size = size;
        return true;
    }
};

#endif //FPGA_DRIVER_SHMBACKEND_H
