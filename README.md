# oscpm

Header-only C++17 OpenSoundControl (OSC) address pattern matching.

oscpm matches OSC address patterns against OSC addresses: the `?`, `*`,
`[...]` and `{a,b}` syntax of OSC 1.0, plus the `//` operator that OSC 1.1
took from XPath. It complements [oscpp](https://github.com/kaoskorobase/oscpp),
which reads and writes OSC packets but leaves address matching to the caller.
oscpm is usable on its own and depends on nothing outside the standard library.

The library is under construction and has no released API yet.

## Building

Builds go through `CMakePresets.json`:

```
cmake --preset release
cmake --build --preset release
ctest --preset release
```

`debug` and `release` use Ninja; `windows` uses Visual Studio 2022. The tests
use Catch2, fetched at configure time; `DEPENDENCIES.md` lists what is fetched
and why.

## Licence

zlib. See `LICENSE`.
