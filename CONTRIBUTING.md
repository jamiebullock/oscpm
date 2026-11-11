# Contributing

## Branches

- `develop` is the integration branch. Every pull request targets it, and it is always buildable.
- `main` holds releases. It is only ever fast-forwarded to a commit on `develop`, so every commit on `main` is also on `develop`.
- A change starts on a branch from `develop` named `<type>/<short-description>` (`feat/`, `fix/`, `perf/`, `docs/`, `ci/`, `chore/`) and is squash-merged by a pull request. A branch is merged once. Further work starts a new branch from `develop`.

## Commits and pull requests

- Commit subjects and pull request titles use Conventional Commit prefixes: `feat:`, `fix:`, `perf:`, `docs:`, `refactor:`, `test:`, `ci:`, `chore:`. The pull request title becomes the commit subject on `develop`.
- A title containing `(MINOR)` or `(MAJOR)` moves that part of the version at the next release. Nothing else affects the version.
- A pull request fills every section of the template. The Summary opens with "This PR" and a verb in lower case, and Changes is one bullet per change.
- `CHANGELOG.md` is written in the pull request that makes a release, by the rules under [Changelog](#changelog). No other pull request changes it.
- Before pushing, build and test locally with the format check on:

  ```sh
  cmake --preset release && cmake --build --preset release && ctest --preset release
  ```

## Benchmark gate

On a pull request, CI counts the instructions per iteration of every benchmark under Cachegrind for the change and for `develop`, and fails when a benchmark rises by more than 2% and 4 instructions. A change that accepts such a rise names the benchmark and its new ceiling, in instructions per iteration as the job's summary reports them, in `bench/accepted.json`. The check passes that benchmark while it stays at or below the ceiling and fails it again above, and it reports an entry that covers no rise past the threshold against `develop`, which is the case once the change has merged, so the entry can be removed.

## Changelog

`CHANGELOG.md` records what each release changes for someone who uses the library. Its entries are written in the pull request that makes the release. The format is [Common Changelog](https://common-changelog.org):

- A release is a `## [X.Y.Z] - YYYY-MM-DD` heading, newest first, with a compare link at the foot of the file.
- Under the heading come `###` sections from Changed, Added, Removed and Fixed, in that order. A release may carry one notice, a single line in italics directly under its heading, and has no other introductory text.
- An entry is one line: a phrase of at most 20 words with no closing full stop.
- An entry opens with a verb in the imperative, such as Add, Change, Fix or Remove. The verbs that the check accepts are listed under [Changelog format](https://github.com/jamiebullock/vale-style#changelog-format) in the vale-style README.
- An entry ends with a link to its pull request, in parentheses: `([#123](https://github.com/jamiebullock/oscpm/pull/123))`.
- A change that can stop a caller's code compiling or alter a result it had opens with `**Breaking:**` and a lower-case verb, and comes first in its section: a removal, a rename, a changed signature, a changed match.
- An entry says what changed for the user. The reason, the mechanism and the measurements are in the pull request.
- Record only what a user can observe: the API, behaviour, results, speed and memory use, the build and install, the licence. Leave out CI, tests, tooling, documentation and refactoring.
- List a new type or function by name. Its contract is documented in the header.
- Fixed holds crashes, memory errors and build failures. A fix that changes what a call returned when it already worked goes under Changed and opens with `**Breaking:**`.
- A deprecation goes under Changed and opens with Deprecate.
- Correct or narrow an entry that is inaccurate. Do not add exceptions and qualifications to it.

An entry that explains the mechanism and the measurements:

```
- Speed up `Pattern::parse` and `Address::parse`, which validate eight bytes at a time. A word holding only plain bytes is accepted with word-wide bit tests, and any other word is checked byte by byte. On Apple M4 with AppleClang 21, `Pattern::parse` takes 0.55x of its previous time.
```

The same change as it belongs in the file:

```
- Speed up `Pattern::parse`, `Address::parse` and `match` ([#77](https://github.com/jamiebullock/oscpm/pull/77))
```

An entry that restates the header:

```
- Add `AddressSpace::find`, which returns a pointer to the value registered under an address, or null when there is none. The pointer is valid until the next `add` or `remove`.
```

The same change as it belongs in the file:

```
- Add `AddressSpace::find` ([#37](https://github.com/jamiebullock/oscpm/pull/37))
```

The `check-prose` target tests the rules that a script can test, and CI runs it on every pull request.

## Prose checks

[Vale](https://vale.sh) checks the prose in the tree: the Markdown files, the changelog, the comments in the sources and the licence headers. The rules come from the [vale-style](https://github.com/jamiebullock/vale-style) package, at the release that `.vale.ini` names, and the package's README describes them. `.vale.ini` adds the project's own settings to them:

- `.vale/styles/config/vocabularies/oscpm/accept.txt` holds the names and terms that the dictionary does not. Add a word there when Vale reports a correct spelling.
- `.vale/styles/oscpm/` holds the rules that apply to this project only.

The `check-prose` target downloads the package and checks every file that git tracks:

```sh
cmake --build --preset release --target check-prose
```

An error fails the check, and CI rejects a pull request that it fails on. A warning is reported and does not fail it. CI runs Vale 3.26.0. `cmake -DVALE=<path> -P cmake/CheckProse.cmake` runs the check with a chosen binary.

CI also checks the title and the description of a pull request: the title as the subject of a commit message, and the description against the template. To run the same check before opening the pull request, write the title to a file named `COMMIT_EDITMSG` and the description to a file named `PR_BODY.md`, both outside the working tree. Then, from the repository root, run `vale sync` and pass the two paths to `vale`.

## Releases

Nothing in the tree records the version. A release is a `vX.Y.Z` tag on `main`, created by `.github/workflows/tag-on-main.yml` on each push to `main`, and CMake reads the latest tag back with `git describe`. The version is the last tag plus one patch, or the next minor or major when a commit subject since the last tag carries `(MINOR)` or `(MAJOR)`. Several such commits still move it once.

To make a release:

1. On `develop`, merge a pull request that adds the release to `CHANGELOG.md`: its heading with the version and the date, its entries and its compare link.
2. Fast-forward `main` to `develop`:

   ```sh
   git push origin develop:main
   ```

   A pull request from `develop` to `main` is not a substitute: the repository squash-merges pull requests, and a squash would put a commit on `main` that `develop` does not have.
3. The workflow tags the new tip of `main`.
4. Pin [oscpm-bench](https://github.com/jamiebullock/oscpm-bench) to the new tag, run its `report.py`, and replace the comparison in the README's Performance section with its output by a pull request on `develop`.
