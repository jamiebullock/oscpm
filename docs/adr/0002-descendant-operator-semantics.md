# `//` matches zero or more parts; trailing `//` and `///` are malformed

OSC 1.1 borrows `//` from XPath but defines it only by example, with a leading `//`. We take XPath's meaning: zero or more whole parts, so `/a//b` matches `/a/b` as well as `/a/x/y/b`. A trailing `//` and any run of three or more slashes are rejected at validation rather than silently failing to match, which is what liblo does and is the kind of quiet failure this library's result codes exist to prevent.

## Considered options

- One-or-more parts: rejected because it contradicts the cited XPath source and makes `/a//b` unable to reach `/a/b`, which surprises users.
- Trailing `//` meaning "all descendants": rejected because no document supports it and it can be expressed as `//*` once the caller decides they want it.
