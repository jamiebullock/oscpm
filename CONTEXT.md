# oscpm

A header-only C++17 library for OSC address pattern matching and pattern-aware address spaces. It complements oscpp, which parses and builds OSC packets but leaves pattern matching to the caller.

## Language

### Addresses

**Address**:
A literal OSC address such as `/synth/1/freq`: a `/`-separated sequence of Parts containing no pattern characters. The thing a Method is registered at.
_Avoid_: Path, route, URL

**Part**:
One `/`-delimited component of an Address or Pattern, at least one byte long. Every Address and Pattern begins with `/` and has at least one Part; the bare root `/`, a trailing `/`, and an empty Part are Malformed. The only permitted empty-looking sequence is the Descendant Operator in a Pattern.
_Avoid_: Segment, component, level, node

**Container**:
A non-terminal Part in the Address Space: a Part that has children.
_Avoid_: Directory, folder, branch

**Method**:
A terminal entry in the Address Space: an Address together with the caller-supplied value stored at it.
_Avoid_: Handler, callback, endpoint, leaf, route

**Address Space**:
The tree of Containers and Methods that Patterns are matched against.
_Avoid_: Namespace, dispatch table, registry, router

### Patterns

**Pattern**:
An OSC address pattern, as carried by an incoming message: an Address in which Parts may contain Wildcards, and which may contain the Descendant Operator. Patterns are matched against Addresses, never against other Patterns.
_Avoid_: Glob, regex, expression, address (when wildcards are possible)

**Wildcard**:
Any of the OSC 1.0 within-Part operators: `?`, `*`, a Character Class, or an Alternative. A Wildcard never matches `/`.
_Avoid_: Metacharacter, special character

**Character Class**:
A bracketed set such as `[abc]`, `[a-z]` or `[!x]`, matching exactly one byte. A leading `!` negates; `-` is literal when first or last.
_Avoid_: Range, set, bracket expression

**Alternative**:
A braced list such as `{foo,bar}`, matching exactly one of its comma-separated literal strings. Members may be empty and may not contain Wildcards or further braces.
_Avoid_: Choice, group, option, brace expression

**Descendant Operator**:
The OSC 1.1 `//` sequence, matching zero or more whole Parts. Permitted at the start of a Pattern or between Parts; not permitted at the end or as `///`.
_Avoid_: Double slash, XPath operator, deep wildcard

### Matching

**Match**:
The yes/no outcome of testing one Pattern against one Address. Distinct from a Malformed result: a Match answer is only given for a well-formed Pattern.
_Avoid_: Hit, dispatch, resolve

**Malformed**:
The state of a Pattern or Address that fails validation, such as an unterminated `[`, a nested `{`, a trailing `//`, or a pattern character in an Address. Reported by result code, never by exception, and never confused with a non-Match.
_Avoid_: Invalid, bad, error (unqualified)

**Lookup**:
Finding every Method in an Address Space whose Address a given Pattern Matches. Delivered to a caller-supplied visitor, one call per Method, allocation-free.
_Avoid_: Dispatch, query, search, route
