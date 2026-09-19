# 06: oscpp adapter, example, README

**What to build:** An oscpp user can build a `Pattern` directly from an oscpp server message and run a Lookup for a message without converting types by hand. The adapter header includes oscpp's server header and is exposed as a second CMake interface target that exists only when oscpp is found, so the core target never carries the dependency. A socket-free example feeds a hard-coded OSC packet through oscpp and the adapter and prints which Methods it reached. The README shows a worked example for the standalone matcher, the Address Space and the adapter, states the real-time guarantees and the lifetime contract, and cites the corpus as the record of every ambiguity decision.

**Blocked by:** 05 Wildcard Lookup

**Status:** done

- [x] Two adapter free functions: one yielding a `Pattern` or error from a message, one running `lookup` for a message
- [x] Adapter CMake target appears when oscpp is located and is silently absent otherwise; core target configuration does not mention oscpp
- [x] Smoke test builds a packet with oscpp's client API, parses it with the server API, and reaches the expected Methods through the adapter
- [x] Example builds with no networking dependency and runs to completion
- [x] README covers install via CMake, the three headers, naming conventions, the no-allocation and no-exception guarantees, the non-owning `Pattern` lifetime rule, and a link to the corpus
- [x] Adapter naming mirrors oscpp style per the interview decision
