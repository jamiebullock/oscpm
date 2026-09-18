# Matching is a set simulation; a Pattern Part is at most 8191 bytes

Within a Part, `*` and Alternatives introduce choice, and the obvious matcher is a backtracking search over those choices. That search is exponential: chained Alternatives whose members are prefixes of one another, such as `{a,}{a,}{a,}…` or `{a,aa}{a,aa}…`, cost the Pattern length raised to the Address length, so a Pattern of a few hundred bytes from the network can stall the audio thread against a short registered Address. Every OSC matcher we consulted has this shape.

We match one Part by simulating the set of Pattern byte positions reachable after each Address byte, as a regular-expression engine without backtracking does. The cost is bounded by the product of the two Part lengths whatever the Pattern contains, and there is no recursion. The set is a fixed bitset on the stack, so the Pattern Part it indexes must have a maximum length: `maxPatternPartLength` is 8191 bytes, the set costs 1 KiB, and `Pattern::parse` reports a longer Part as `PartTooLong` at the first byte beyond the limit. The limit sits on the untrusted side: an Address Part has no limit, and 8191 bytes is far beyond any Part an OSC client sends.

## Considered options

- Backtracking with the "last `*`" shortcut: polynomial for `*` alone, but still exponential once Alternatives are chained; the hostile-input test in `pattern_test.cpp` hangs on it.
- Backtracking with recursion bounded by the Address length: bounds the stack but not the time.
- A bitset over Address positions instead: the same algorithm with the limit on the Address side, rejected because it would reject a caller's own registered Address rather than hostile input.
- A heap-allocated set with no limit: rejected because matching must not allocate (ADR 0003).
