# 04: Address Space with exact Lookup

**What to build:** A consumer can create an `AddressSpace<T>`, register Methods at literal Addresses with a value of their own type, remove them, walk them in address order, and look up a Pattern that contains no Wildcards. `add` moves the value in and rejects Malformed Addresses (pattern characters or structural faults) and Duplicates; `remove` reports NotFound. An Address may be both a Container and a Method. `lookup` with a Wildcard-free Pattern takes the binary-search fast path and calls the visitor as `(address, T&)` once, returning the count; a const overload passes `const T&`. `forEach` visits every Method in address order. Registration may allocate; Lookup and `forEach` may not.

**Blocked by:** 01 Scaffold and literal matching

**Status:** done

- [x] `AddressSpace<T>` compiles with a move-only T such as `std::unique_ptr` and with a capturing lambda type
- [x] `add` returns Malformed with offset for pattern characters and structural faults; Duplicate on a second registration at the same Address
- [x] `remove` returns NotFound for an unknown Address and succeeds otherwise; removing a Method that is also a Container leaves its children intact
- [x] `/synth` and `/synth/freq` coexist and both are found by exact Lookup
- [x] `lookup` returns 0 and does not call the visitor for an unregistered exact Address
- [x] `forEach` order is address order, proven with an interleaved registration order
- [x] Zero allocations during `lookup` and `forEach`, asserted with a counting `operator new`
- [x] A Pattern containing Wildcards is accepted by `lookup` but, for this ticket only, may return 0; documented in the test as pending ticket 05
