# Header-only C++17 with no dependencies, independent of oscpp

oscpm is a single header requiring C++17 and nothing else. It does not depend
on oscpp even though it is designed to complement it: the only thing it needs
from a message is the address, and `std::string_view` carries that from any
OSC library. C++17 rather than oscpp's C++11 was chosen for `string_view` and
`constexpr` matching, accepting that projects stuck on older standards cannot
use it.
