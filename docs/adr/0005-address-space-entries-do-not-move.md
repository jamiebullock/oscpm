# Address Space entries are heap allocated once and never move

`AddressSpace<T>` keeps each Container or Method as its own heap allocation, with children held through `std::unique_ptr` in a sorted vector. Registration therefore allocates once per new Part, which the spec permits, and no later registration or removal moves an existing entry. Two things follow that consumers can rely on: the `T&` and the Address `string_view` a visitor receives stay valid until that Method is removed or the Address Space is destroyed, and `T` need only be move-constructible, since nothing ever move-assigns it. The stability matters once the span-filling Lookup convenience hands out `T*`, and it lets a handler keep a pointer to its own value across further setup.

## Considered options

- Entries stored by value in the parent's vector: one allocation fewer per level and better locality on Lookup, but every insertion shifts siblings, so references and pointers to values dangle after the next `add`, and `T` would need to be move-assignable, which rules out a capturing lambda type.
- A flat sorted vector of full Addresses: simplest to walk in address order, but the same invalidation on insert, and the wildcard Lookup of ticket 05 needs the tree to bound `//` backtracking to the Parts that exist.
