//
// Created by asherjil on 2/1/26.
//

#include "BackendBase.hpp"
#include <sys/mman.h> // for ::mmap/unmap()
#include <fcntl.h> // for ::open()
#include <unistd.h> // for ::close()

BackendBase::BackendBase(BackendBase&& other) noexcept
    :m_fd{other.m_fd}, m_baseAddress{other.m_baseAddress}, m_size {other.m_size} {

    other.m_fd = -1;
    other.m_baseAddress = nullptr;
    other.m_size = 0;
}
BackendBase::BackendBase& operator=(BackendBase&& other) noexcept {

    if (this != &other) {
        close();// close the connection
        // steal the data
        m_fd = other.m_fd;
        m_baseAddress = other.m_baseAddress;
        m_size = other.m_size;

        // set the other to empty/nulls
        other.m_fd = -1;
        other.m_baseAddress = nullptr;
        other.m_size = 0;
    }
    return *this;
}

BackendBase::~BackendBase() {
    close(); // close connection, easier to debug the close function than the destructor
}

bool BackendBase::open(const char* resource, std::size_t size) {
    if (isOpen()) {
        return false;
    }

    // resource -> path to device file PCIe BAR or AXI region
    // O_RDWR -> Read/Write, O_SYNC -> Synchronous I/O
    m_fd = ::open(resource, O_RDWR | O_SYNC);
    if (m_fd == -1) {
        return false;
    }

    m_baseAddress = ::mmap(
        nullptr,// find anywhere in the virtual memory address space
        size, // number of bytes to map
        PROT_READ | PROT_WRITE, // CPU permission to read/write in this region
        MAP_SHARED | MAP_LOCKED, // MAP_SHARED -> memory write is immediately available to the hardware(FPGA)
        // MAP_LOCKED -> to prevent "first-access" page faults
        m_fd, // file descriptor for the device driver
        0); // offset, 0 means start of the memory region

    if (m_baseAddress == MAP_FAILED) {
        ::close(m_fd);
        m_fd = -1;
        m_baseAddress = nullptr;
        return false;
    }

    m_size = size;
    return true;
}

bool BackendBase::isOpen() const noexcept {
    return m_baseAddress != nullptr && m_baseAddress != MAP_FAILED;
}

void* BackendBase::getBaseAddress() const noexcept {
    return m_baseAddress;
}

std::size_t BackendBase::getMmapSize() const noexcept {
    return m_size;
}

void BackendBase::close() {
    if (isOpen()) {
        ::munmap(m_baseAddress, m_size);
    }
    if (m_fd != -1) {
        ::close(m_fd);
    }
    m_fd = -1;
    m_baseAddress = nullptr;
    m_size = 0;
}
