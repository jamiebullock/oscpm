# 02: Within-Part Wildcards

**What to build:** Patterns using the OSC 1.0 within-Part operators match as the spec and the interview decisions say. `?` matches exactly one byte; `*` matches zero or more bytes and never `/`; a Character Class matches one byte with ranges in ASCII order, leading `!` negation, and `-` literal when first or last; an Alternative matches exactly one of its comma-separated literal members, an empty member matches the empty string, and characters inside braces are literal. Nested braces, brackets inside braces, `[]`, `[!]`, and an unterminated `[` or `{` are Malformed with the offset of the offending byte. Matching is byte-wise and case-sensitive with no escaping.

**Blocked by:** 01 Scaffold and literal matching

**Status:** done

- [x] Corpus sections for `?`, `*`, Character Class, Alternative, each with positive and negative cases and the `/` boundary
- [x] Corpus section for every Q11 and Q12 decision: nesting rejected, empty member matches empty, brace contents literal, `-` edge rules, `[]` `[!]` unterminated forms
- [x] `*` followed by literal text backtracks correctly, e.g. `/a*b` against `/aXbYb`
- [x] The "not yet supported" error kind from ticket 01 is gone
- [x] Still no exceptions and no allocation in `parse` or `matches`
