// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 10/22/26.
//

#ifndef ABTEDGE_SEQLOCK_HPP
#define ABTEDGE_SEQLOCK_HPP

#include "common/CacheLine.hpp"
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <cstring>

template<typename T>
concept SeqLockPayload = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;


using sequence_type = std::uint64_t;

template<SeqLockPayload T>
class alignas(CACHE_LINE_SIZE) SeqLock{
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
public:

    // The rule of 5
    constexpr SeqLock() noexcept       = default;
    SeqLock(const SeqLock&)            = delete;
    SeqLock& operator=(const SeqLock&) = delete;
    SeqLock(SeqLock&&)                 = delete;
    SeqLock& operator=(SeqLock&&)      = delete;
    ~SeqLock()                         = default;

    // Writer side
    void store(const T& value) noexcept;

    // Reader side
    [[nodiscard]] sequence_type load(T& out, std::uint64_t& retries) const noexcept;
    [[nodiscard]] sequence_type load(T& out) const noexcept;
    [[nodiscard]] sequence_type try_load(T& out, sequence_type known) const noexcept;
    [[nodiscard]] sequence_type sequence() const noexcept;
private:
    std::atomic<sequence_type> m_seq{0};
    T m_payload{};
};

template<SeqLockPayload T>
void SeqLock<T>::store(const T& value) noexcept{
    const sequence_type seq = m_seq.fetch_add(1, std::memory_order_acq_rel);
    std::memcpy(&m_payload, &value, sizeof(T)); // copy the data into payload
    m_seq.store(seq + 2, std::memory_order_release); // increment the sequence by two in this function
}

template<SeqLockPayload T>
sequence_type SeqLock<T>::load(T& out) const noexcept{
    std::uint64_t unused = 0; // dead after inlining
    return load(out, unused);
} 

template<SeqLockPayload T>
sequence_type SeqLock<T>::load(T& out, std::uint64_t& retries) const noexcept{
    while (true){
        const sequence_type seq0 = m_seq.load(std::memory_order_acquire);

        if (seq0 & 1u){
            ++retries;
            continue; // keep spinning
        }

        std::memcpy(&out, &m_payload, sizeof(T));
        std::atomic_thread_fence(std::memory_order_acquire);

        if (const sequence_type seq1 = m_seq.load(std::memory_order_relaxed); seq0 == seq1){
            return seq0;
        }
        ++retries;
    }
}


template<SeqLockPayload T>
sequence_type SeqLock<T>::try_load(T& out, sequence_type known) const noexcept{
    if (m_seq.load(std::memory_order_relaxed) == known){
        return known;
    }

    return load(out);
}

template<SeqLockPayload T>
sequence_type SeqLock<T>::sequence() const noexcept{
    return m_seq.load(std::memory_order_acquire);
}

template <SeqLockPayload T>
constexpr bool seqlock_layout_ok_v = alignof(SeqLock<T>) == CACHE_LINE_SIZE &&
                                    sizeof(SeqLock<T>) % CACHE_LINE_SIZE == 0 &&
                                    std::is_trivially_destructible_v<SeqLock<T>>;




#endif // ABTEDGE_SEQLOCK_HPP
