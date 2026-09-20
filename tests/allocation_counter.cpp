/* Part of oscpm
 *
 * SPDX-FileCopyrightText: 2026 Jamie Bullock
 * SPDX-License-Identifier: Zlib
 */

#include "allocation_counter.h"

#include <cstdlib>
#include <new>

namespace
{

std::size_t g_numAllocations = 0;

void* countedMalloc(std::size_t size) noexcept
{
    ++g_numAllocations;
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
