# oscpm

A library that decides whether an OSC address pattern matches an OSC address.
It exists because oscpp handles OSC packets but not this decision.

## Language

**Address**:
The name of a destination inside an OSC server, written as a `/`-separated path.
_Avoid_: Path, route, endpoint

**Pattern**:
The address-shaped text carried by an OSC message, which may contain operators and so match many addresses.
_Avoid_: Glob, regex, address pattern (when the distinction from Address matters, say Pattern)

**Part**:
One segment of an address or pattern, delimited by `/`.
_Avoid_: Component, container, method name, segment

**Operator**:
A character sequence in a pattern that stands for something other than itself.
_Avoid_: Wildcard (that is one kind of operator), metacharacter

**Character class**:
The `[...]` operator, matching one character from a listed set or range, or outside it when negated with `!`.
_Avoid_: Bracket expression, set

**Alternatives**:
The `{...}` operator, matching one of several literal strings.
_Avoid_: Braces, choice, group

**Descendant operator**:
The `//` operator from OSC 1.1, matching zero or more whole parts.
_Avoid_: Double slash, XPath operator

**Literal pattern**:
A pattern with no operators, which matches exactly one address.
_Avoid_: Plain pattern, exact pattern

**Malformed pattern**:
A pattern that oscpm refuses to interpret, and which therefore matches nothing.
_Avoid_: Invalid pattern, bad pattern

**Matching path**:
The code that runs when a pattern is compared with an address, as opposed to when a pattern is built.
_Avoid_: Hot path, fast path
