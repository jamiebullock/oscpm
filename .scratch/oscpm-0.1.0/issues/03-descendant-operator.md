# 03: Descendant Operator

**What to build:** Patterns using the OSC 1.1 `//` match zero or more whole Parts, as ADR 0002 records. `//gain` reaches `/gain`, `/a/gain` and `/a/b/gain`; `/a//b` matches `/a/b` and `/a/x/y/b`. `//` is permitted leading and between Parts and combines with every within-Part Wildcard on either side with correct backtracking. A trailing `//` and any run of three or more slashes are Malformed with the offset of the offending slash.

**Blocked by:** 02 Within-Part Wildcards

**Status:** ready-for-agent

- [ ] Corpus section for every Q10 decision: leading, middle, zero-part, multi-part, trailing rejected, `///` rejected
- [ ] Corpus cases combining `//` with `*`, `?`, Character Class and Alternative on both sides, including cases that require backtracking past a false first match
- [ ] Multiple `//` in one Pattern, e.g. `//a//b`
- [ ] Still no exceptions and no allocation in `parse` or `matches`
