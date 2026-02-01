//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_BACKENDBASE_HPP
#define ABTEDGE_BACKENDBASE_HPP

#include <cstddef> // for std::size_t
#include <cstdint> // std::uint8_t

class BackendBase {
public:
    // Rule of 5 implementation for a move-only object
    BackendBase() = default;
    BackendBase(const BackendBase&) = delete;
    BackendBase(BackendBase&& other) noexcept;
    BackendBase& operator=(const BackendBase&) = delete;
    BackendBase& operator=(BackendBase&& other) noexcept;
    ~BackendBase();

    [[nodiscard]] bool open(const char* resource, std::size_t size);
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] void* getBaseAddress() const noexcept;
    [[nodiscard]] std::size_t getMmapSize() const noexcept;

    void close();
protected:
    // Get the register pointer at the given offset. This template function must stay in the .hpp file.
    template<typename T>
    [[nodiscard]] volatile T* registerPtr(std::size_t offset) const noexcept {
        return reinterpret_cast<volatile T*>(static_cast<std::uint8_t*>(m_baseAddress) + offset);
    }

    int m_fd{-1};
    void* m_baseAddress{nullptr};
    std::size_t m_size{0};
};


#endif //ABTEDGE_BACKENDBASE_HPP