/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <limits>
#include <new>

namespace
{

constexpr std::size_t kNever = std::numeric_limits<std::size_t>::max();

std::size_t g_numAllocations = 0;
std::size_t g_failingAllocation = kNever;

void* countedMalloc(std::size_t size) noexcept
{
    if (g_numAllocations++ == g_failingAllocation)
    {
        return nullptr;
    }
    return std::malloc(size == 0 ? 1 : size);
}

void* allocateOrThrow(std::size_t size)
{
    if (void* memory = countedMalloc(size))
    {
        return memory;
    }
    throw std::bad_alloc();
}

}

namespace oscpm_test
{

std::size_t allocationCount() noexcept
{
    return g_numAllocations;
}

FailedAllocation::FailedAllocation(std::size_t numAllowed) noexcept
{
    g_failingAllocation = g_numAllocations + numAllowed;
}

FailedAllocation::~FailedAllocation()
{
    g_failingAllocation = kNever;
}

}

void* operator new(std::size_t size)
{
    return allocateOrThrow(size);
}

void* operator new[](std::size_t size)
{
    return allocateOrThrow(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return countedMalloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return countedMalloc(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

TEST_CASE("the allocation counter observes the heap")
{
    const std::size_t before = oscpm_test::allocationCount();
    void* const allocated = ::operator new(sizeof(int));
    CHECK(oscpm_test::allocationCount() > before);
    ::operator delete(allocated);
}
