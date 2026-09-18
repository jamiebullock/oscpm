# 07: Fuzzing under sanitizers

**What to build:** A maintainer can build a libFuzzer target that feeds arbitrary bytes to `Pattern::parse` and `match`, under AddressSanitizer and UndefinedBehaviorSanitizer, and run it locally for as long as they like. The target is seeded from the conformance corpus so it starts from meaningful shapes. Any crash or sanitizer report is a bug in the scanner.

**Blocked by:** 03 Descendant Operator

**Status:** ready-for-agent

- [ ] CMake option enables the fuzz target only on compilers that support libFuzzer, and it is off by default
- [ ] Fuzz entry splits its input into a Pattern and an Address and exercises `parse`, `matches` and `validateAddress`
- [ ] Seed corpus generated from the conformance corpus at configure or build time
- [ ] Documented one-line command to build and run under ASan and UBSan
- [ ] A ten-minute local run completes with no findings; any findings fixed and added to the conformance corpus as regression cases
