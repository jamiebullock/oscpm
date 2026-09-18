#pragma once

#include <cstddef>

// Counts calls to the global allocation functions, which the test binary
// replaces. Use `allocationCount()` deltas to assert that a code path did
// not touch the heap.
namespace oscpm_test {

std::size_t allocationCount() noexcept;

}
