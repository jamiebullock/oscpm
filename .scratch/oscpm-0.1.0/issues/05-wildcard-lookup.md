# 05: Wildcard Lookup

**What to build:** A Lookup with any valid Pattern, including every within-Part Wildcard and the Descendant Operator, delivers every matching Method to the visitor exactly once and returns the count. The tree walk backtracks correctly for `*`, Alternatives and `//`. A span-filling convenience collects `(address, T*)` results into caller-provided storage and reports how many fit. The full conformance corpus, replayed through a populated Address Space, gives the same answers as the standalone matcher.

**Blocked by:** 03 Descendant Operator, 04 Address Space with exact Lookup

**Status:** ready-for-agent

- [ ] Corpus replay test: every corpus Address is registered, every corpus Pattern is looked up, and the visited set equals the set of Addresses the matcher says Match
- [ ] Each matching Method is visited exactly once even when several Wildcard paths reach it, e.g. `//a//b` over `/a/a/b`
- [ ] Visitor order is address order, matching `forEach`
- [ ] Span convenience truncates without error when storage is too small and reports the true count separately
- [ ] The pending note from ticket 04 is removed
- [ ] Zero allocations during wildcard `lookup`, including deep `//` walks
