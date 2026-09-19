# No allocation or exceptions in the matching path; malformed patterns match nothing

Matching is expected to run inside audio and control threads, so the matching
path must not allocate, throw, or otherwise block. This rules out the
conventional pattern-compiles-to-a-heap-structure design: `Pattern` keeps the
validated text and matches by recursive descent over it, with stack depth
bounded by the number of operators in the pattern.

For the same reason errors are values, not exceptions. A malformed pattern is
rejected outright rather than interpreted leniently, because a guessed
interpretation of `/a[b` would differ silently between implementations. A
rejected pattern reports why through `Error` and matches nothing.

## Consequences

- Constructing a `Pattern` copies the text and may allocate. Build patterns
  at setup time, not in the matching path.
- Backtracking is exponential in the worst case for pathological patterns.
  This is accepted because OSC patterns are short and untrusted-input
  hardening is not a goal.
