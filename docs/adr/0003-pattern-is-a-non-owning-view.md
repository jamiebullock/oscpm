# `Pattern` is validated once and does not own its bytes

A malformed pattern must be reported as Malformed, never as a non-match, but validating on every `match` call costs a second pass and lazy validation gives different answers depending on how far the scan gets. We therefore make `Pattern::parse` the single validation point, returning a `Pattern` whose `matches` is total and single-pass. The `Pattern` holds only a `std::string_view`: the caller owns the bytes for as long as the `Pattern` is used, which is the same contract oscpp applies to its message views and is what keeps matching allocation-free on audio threads. A bare `match(pattern, address)` convenience does parse-then-match for callers who do not care about the cost.

## Considered options

- Validate fully inside every `match` call: correct but two passes per call.
- Validate lazily during the scan: single pass, but the same malformed pattern reports NoMatch against one address and Malformed against another.
- An owning `Pattern` holding a `std::string`: simpler lifetime, but allocates, and the library's hot path must not.
