# Implement OSC 1.0 pattern matching plus the OSC 1.1 descendant operator

OSC 1.0 defines `?`, `*`, `[]` and `{}`; OSC 1.1 adds only `//`, and the rest
of 1.1 is unrelated to matching. We implement 1.0 in full and take `//` from
1.1 because it is cheap to add and widely expected, while stopping there keeps
the syntax a strict superset of 1.0 that every OSC client can rely on. Nothing
beyond the two specifications (such as regex-style operators) will be added.

## Consequences

- `//` matches zero or more whole parts, so `//foo` matches `/foo`. Some
  implementations require at least one part; ours follows the OSC 1.1 paper.
- Alternatives inside `{}` are literal text. Neither specification says
  whether operators nest inside braces, and rejecting them avoids defining
  behaviour that other implementations would not share.
