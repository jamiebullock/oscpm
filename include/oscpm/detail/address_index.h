/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace oscpm::detail
{

inline std::size_t hashOf(std::string_view text) noexcept
{
    return std::hash<std::string_view> { }(text);
}

class AddressIndex
{
public:
    void reserveForInsert()
    {
        if (m_hashes.size() == m_hashes.capacity())
        {
            m_hashes.reserve(2 * m_hashes.size() + 1);
        }
        const std::size_t numSlotsNeeded = kSlotsPerEntry * (m_hashes.size() + 1);
        if (m_slots.size() < numSlotsNeeded)
        {
            growSlots(numSlotsNeeded);
        }
    }

    /// Adds an entry at `index`. `reserveForInsert` must have been called
    /// since the last `insert` into this object or assignment to it.
    void insert(std::size_t index, std::size_t hash) noexcept
    {
        m_hashes.insert(m_hashes.begin() + static_cast<std::ptrdiff_t>(index), hash);
        const auto shifted = static_cast<std::uint32_t>(index);
        for (std::uint32_t& slot : m_slots)
        {
            slot += slot > shifted ? 1U : 0U;
        }
        const std::size_t mask = m_slots.size() - 1;
        std::size_t slot = hash & mask;
        while (m_slots[slot] != kEmptySlot)
        {
            slot = (slot + 1) & mask;
        }
        m_slots[slot] = static_cast<std::uint32_t>(index + 1);
    }

    void erase(std::size_t index) noexcept
    {
        eraseSlot(index);
        m_hashes.erase(m_hashes.begin() + static_cast<std::ptrdiff_t>(index));
    }

    template <typename IsEntry>
    bool find(std::size_t hash, IsEntry isEntry, std::size_t& index) const
    {
        if (m_slots.empty())
        {
            return false;
        }
        const std::size_t mask = m_slots.size() - 1;
        for (std::size_t slot = hash & mask; m_slots[slot] != kEmptySlot; slot = (slot + 1) & mask)
        {
            const std::size_t candidate = m_slots[slot] - 1;
            if (m_hashes[candidate] == hash && isEntry(candidate))
            {
                index = candidate;
                return true;
            }
        }
        return false;
    }

private:
    static constexpr std::size_t kMinSlots = 16;
    static constexpr std::size_t kSlotsPerEntry = 2;
    static constexpr std::uint32_t kEmptySlot = 0;

    void growSlots(std::size_t numSlotsNeeded)
    {
        std::size_t numSlots = kMinSlots;
        while (numSlots < numSlotsNeeded)
        {
            numSlots *= 2;
        }
        std::vector<std::uint32_t> slots(numSlots, kEmptySlot);
        for (std::size_t index = 0; index < m_hashes.size(); ++index)
        {
            std::size_t slot = m_hashes[index] & (numSlots - 1);
            while (slots[slot] != kEmptySlot)
            {
                slot = (slot + 1) & (numSlots - 1);
            }
            slots[slot] = static_cast<std::uint32_t>(index + 1);
        }
        m_slots.swap(slots);
    }

    void eraseSlot(std::size_t index) noexcept
    {
        const std::size_t mask = m_slots.size() - 1;
        std::size_t hole = m_hashes[index] & mask;
        while (m_slots[hole] != index + 1)
        {
            hole = (hole + 1) & mask;
        }
        for (std::size_t next = (hole + 1) & mask; m_slots[next] != kEmptySlot; next = (next + 1) & mask)
        {
            const std::size_t home = m_hashes[m_slots[next] - 1] & mask;
            const std::size_t distanceFromHome = (next - home) & mask;
            const std::size_t distanceFromHole = (next - hole) & mask;
            if (distanceFromHome >= distanceFromHole)
            {
                m_slots[hole] = m_slots[next];
                hole = next;
            }
        }
        m_slots[hole] = kEmptySlot;
        const auto removed = static_cast<std::uint32_t>(index + 1);
        for (std::uint32_t& slot : m_slots)
        {
            slot -= slot > removed ? 1U : 0U;
        }
    }

    std::vector<std::size_t> m_hashes;
    std::vector<std::uint32_t> m_slots;
};

}
