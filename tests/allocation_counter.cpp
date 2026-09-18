#include "allocation_counter.hpp"

#include <cstdlib>
#include <new>

namespace {

std::size_t g_allocations = 0;

void* allocate(std::size_t size)
{
    ++g_allocations;
    if (size == 0) {
        size = 1;
    }
    if (void* p = std::malloc(size)) {
        return p;
    }
    throw std::bad_alloc();
}

} // namespace

namespace oscpm_test {

std::size_t allocationCount() noexcept
{
    return g_allocations;
}

} // namespace oscpm_test

void* operator new(std::size_t size)
{
    return allocate(size);
}

void* operator new[](std::size_t size)
{
    return allocate(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    ++g_allocations;
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    ++g_allocations;
    return std::malloc(size == 0 ? 1 : size);
}

void operator delete(void* p) noexcept
{
    std::free(p);
}

void operator delete[](void* p) noexcept
{
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}
