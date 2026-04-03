//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_BACKENDBASE_HPP
#define ABTEDGE_BACKENDBASE_HPP

#include <cstddef> // for std::size_t
#include <cstdint> // for std::uint8_t
#include <type_traits> // for std::is_same_v

class BackendBase {
public:
	// Rule of 5 implementation for a move-only object
	BackendBase() = default;
	BackendBase(const BackendBase&) = delete;
	BackendBase(BackendBase&& other) noexcept;
	BackendBase& operator=(const BackendBase&) = delete;
	BackendBase& operator=(BackendBase&& other) noexcept;
	~BackendBase();

	[[nodiscard]] bool open(const char* resource, std::size_t physicalAddress, std::size_t size);
	[[nodiscard]] bool isOpen() const noexcept;
	[[nodiscard]] void* getBaseAddress() const noexcept;
	[[nodiscard]] std::size_t getMmapSize() const noexcept;

	// Use this function directly for read/write. No need for wrappers or excessive function calls.
	template<typename T>
	[[nodiscard, gnu::always_inline]]
	inline volatile T* registerPtr(std::size_t offset) const noexcept {
	    return reinterpret_cast<volatile T*>(reinterpret_cast<std::uint8_t*>(m_baseAddress) + offset);
	}

	template<auto Mask>
	[[nodiscard, gnu::always_inline]]
	inline auto readFromField(std::size_t offset) const noexcept{
		using T = decltype(Mask);
		static_assert(std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);

		constexpr unsigned shift = (std::is_same_v<T, std::uint32_t>) ? __builtin_ctz(Mask) : __builtin_ctzll(Mask);

		T regValue = *registerPtr<T>(offset); // read the current register
		return (regValue & Mask) >> shift; // return the field from the register using the mask
	}

	template<auto Mask>
	[[gnu::always_inline]]
	inline void writeToField(std::size_t offset, decltype(Mask) value) noexcept{
		using T = decltype(Mask);
		static_assert(std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);

		constexpr unsigned shift = (std::is_same_v<T, std::uint32_t>) ? __builtin_ctz(Mask) : __builtin_ctzll(Mask);

		T currentValue = *registerPtr<T>(offset); // read the current value from the register
		T newValue     = (currentValue & ~Mask) | ((value << shift) & Mask); // perform the masking
		*registerPtr<T>(offset) = newValue; // write to the register
	}

	void close();
protected:
	int m_fd{-1};
	void* m_baseAddress{nullptr};
	std::size_t m_size{0};
};


#endif //ABTEDGE_BACKENDBASE_HPP