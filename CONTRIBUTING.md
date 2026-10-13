# Contributing

## Branches

- `develop` is where changes land. Every pull request targets it, and it is always buildable.
- `main` holds releases. It is only ever fast-forwarded to a commit on `develop`, so every commit on `main` is also on `develop`.
- A change starts on a branch from `develop` named `<type>/<short-description>` (`feat/`, `fix/`, `perf/`, `docs/`, `ci/`, `chore/`) and is squash-merged by a pull request. A branch is merged once; further work starts a new branch from `develop`.

## Commits and pull requests

- Commit subjects and pull request titles use Conventional Commit prefixes: `feat:`, `fix:`, `perf:`, `docs:`, `refactor:`, `test:`, `ci:`, `chore:`. The pull request title becomes the commit subject on `develop`.
- A title containing `(MINOR)` or `(MAJOR)` moves that part of the version at the next release. Nothing else affects the version.
- A pull request fills every section of the template.
- A pull request that changes behaviour, a public header or the build adds an entry under the `Unreleased` heading of `CHANGELOG.md`: breaking changes first, then Added, Changed and Fixed.
- Before pushing, build and test locally with the format check on:

  ```sh
  cmake --preset release && cmake --build --preset release && ctest --preset release
  ```

## Releases

Nothing in the tree records the version. A release is a `vX.Y.Z` tag on `main`, created by `.github/workflows/tag-on-main.yml` on each push to `main`, and CMake reads the latest tag back with `git describe`. The version is the last tag plus one patch, or the next minor or major when a commit subject since the last tag carries `(MINOR)` or `(MAJOR)`; several such commits still move it once.

To make a release:

1. On `develop`, merge a pull request that renames the `Unreleased` heading of `CHANGELOG.md` to that version, with the date.
2. Fast-forward `main` to `develop`:

   ```sh
   git push origin develop:main
   ```

   A pull request from `develop` to `main` is not a substitute: the repository squash-merges pull requests, and a squash would put a commit on `main` that `develop` does not have.
3. The workflow tags the new tip of `main`.
