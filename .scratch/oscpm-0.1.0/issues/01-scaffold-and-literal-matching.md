# 01: Scaffold and literal matching

**What to build:** A consumer can add oscpm as a CMake interface target, build the test suite, and match literal Patterns against Addresses. `Pattern::parse`, `Pattern::matches`, the bare `match` convenience and `validateAddress` exist and enforce every structural rule from the glossary: leading slash required, at least one Part, no empty Parts, no trailing slash, bare root is Malformed. Malformed results carry an error kind and byte offset. A plain-text conformance corpus exists with one case per line, is loaded by a single Catch2 test, and passes for the literal and structural sections. No Wildcards yet; a Pattern containing any pattern character is Malformed for now with a clearly named "not yet supported" kind that later tickets remove.

**Blocked by:** None (can start immediately)

**Status:** ready-for-agent

- [ ] CMake interface target builds header-only with C++17, Catch2 v3 pulled via FetchContent, tests run with `ctest`
- [ ] Corpus file format documented at its head; loader reports the line number of any failing case
- [ ] `Pattern::parse` returns either a trivially copyable non-owning `Pattern` or an error with kind and byte offset (ADR 0003)
- [ ] `matches` returns a tri-state result; `match(pattern, address)` does parse-then-match
- [ ] `validateAddress` rejects pattern characters `? * [ ] { } , #` and space, and every structural rule, each with the right offset
- [ ] Corpus sections cover: exact match, mismatch, structural Malformed for both Pattern and Address
- [ ] No exceptions, no allocation in `parse` or `matches` (asserted with a counting `operator new` in the test)
- [ ] Clean at `-Wall -Wextra -Wpedantic` on the local compiler
