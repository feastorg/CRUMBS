# Contributing to CRUMBS

## Build and test

The core builds anywhere with a C11 compiler and CMake 3.13 (3.21 to use the
presets). Nothing else is needed for the tests.

```sh
cmake --preset default          # or: cmake -S . -B build
cmake --build --preset default
ctest --preset default          # or: ctest --test-dir build --output-on-failure
```

Presets: `default` (Debug, tests), `release`, and `linux` (Debug, tests, Linux
HAL and the six Linux example programs — needs linux-wire ≥ 0.1.3 on
`CMAKE_PREFIX_PATH`; see [platform-setup.md](docs/platform-setup.md#linux)).
Each configures into its own `build*/` directory.

Options: `CRUMBS_ENABLE_LINUX_HAL` (off), `CRUMBS_BUILD_EXAMPLES` (on, but only
built when the HAL is on), `CRUMBS_ENABLE_TESTS` (on). Two configurations CI
also runs, with the flags given to both `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS`:
`-DCRUMBS_MAX_HANDLERS=0` (two tests then skip with exit 77) and ASan+UBSan.

Tests are one binary per `tests/test_*.c` (or `.cpp`), registered in the root
`CMakeLists.txt`; `tests/test_common.h` has the assertion macros. Both HALs
run on the host against a scripted bus: `test_linux_hal` compiles the Linux
HAL against `tests/fake_linux_wire.c` (registered under
`if(CRUMBS_ENABLE_LINUX_HAL)`, since it needs the linux-wire header), and
`test_arduino_hal` compiles the Arduino HAL against the `Arduino.h` and
`Wire.h` in `tests/fake_arduino/` (needs a C++ compiler; skipped without one).
The real sketches are compiled by the `arduino-cli` and `platformio` CI jobs.
`tests/compile_fail/*.c` must each *fail* to compile with the message on their
first line; CTest builds them on demand and matches the compiler's output, so
`ctest -R compile_fail` is how the `crumbs_ops.h` static checks are tested.

Arduino sketches compile with the tree as the library:

```sh
arduino-cli compile --fqbn arduino:avr:nano --warnings more --library "$PWD" examples/core_usage/arduino/hello_peripheral
```

PlatformIO examples pin the published package (`lib_deps =
cameronbrooks11/CRUMBS@^x.y.z`), so `pio run` in them builds the registry
version, not your checkout. To build a PlatformIO project against the tree,
set `lib_deps = symlink:///path/to/CRUMBS` in a scratch copy.

## Documentation checks

```sh
./scripts/doccheck.sh                 # Doxygen over the public headers; fails on any warning
python3 scripts/check_api_index.py    # every public symbol is in docs/api-reference.md
python3 scripts/check_docs_links.py   # every relative Markdown link and #anchor resolves
```

All three run in CI. The gate fails on a public symbol with no Doxygen
comment, a malformed one, or a header without a `@file` block. The
convention it does not enforce: `@brief`, `@param` for every parameter,
`@return` listing the codes, and a note wherever a pointer may be `NULL`.
Internal `static` functions in `.c` files get ordinary comments that say why,
not what.

The docs follow one rule: **one home per fact**. Signatures and return codes
live in the headers; the wire format lives in `docs/protocol.md`; everything
else links to them. A number in a doc carries the command that produced it.

## Code style

- C11 in `src/core` and `src/crc`; C++ only in the Arduino HAL. No heap, no
  platform includes in the core.
- 4-space indent, Allman braces as in the existing files, `crumbs_` prefix on
  every public symbol, `snake_case`. Return `0` on success and a negative code
  on failure; scanners return a count.
- C, C++ and sketch files are plain ASCII, comments and strings included:
  `I2C` not `I²C`, `-` not `–`. Markdown may use typography.
- `src/crc/crc8_nibble.{c,h}` is generated: `python3 scripts/generate_crc8.py`
  (pycrc 0.11.0) regenerates it and CI diffs the result against the tree.
  Other variants (`--algos bit,nibble,nibblem,byte --no-stage`) go to `dist/`
  and are not built.

## Commits and pull requests

Conventional Commits: `type(scope): subject` with `type` one of `feat`, `fix`,
`docs`, `refactor`, `test`, `ci`, `chore`, `build`; imperative, lowercase, no
trailing period, ≤ 72 characters. Reference issues in the body or footer
(`Closes #N`), one logical change per commit.

Branch from `main`, open a PR, and let CI finish: the single required check
is `ok`, which depends on every job. PRs are squash-merged with the PR body as
the message, so write the body as the commit you want in history: what
changed, why, and how it was verified. Add a `[Unreleased]` entry to
`CHANGELOG.md` for anything a user would notice.

## CI

`ci.yml` runs on pushes and PRs to `main` and `dev`:

| Job | What it proves |
| --- | --- |
| `core-only-install` | Core build, tests, install, and a `find_package(crumbs)` consumer without the HAL; `crumbs_linux.h` must not leak into that install |
| `build-and-test` | Linux HAL build and tests against the pinned, checksum-verified linux-wire release; install; the four out-of-tree example builds and the five in-tree ones (which must pull in the library only); a smoke run of `crumbs_simple_linux_controller` |
| `platformio` | The eight PlatformIO projects for `nanoatmega328new` and `esp32dev` — against the registry package |
| `arduino-cli` | The two mixed-bus Arduino sketches with ezo-driver and SparkFun BME280 pinned by tag and SHA |
| `build-configs` | Core and Arduino-HAL host tests under ASan+UBSan and with `CRUMBS_MAX_HANDLERS=0`, the flags given to both languages |
| `doxygen` | `doccheck.sh` and `check_api_index.py` (pinned to `ubuntu-24.04` for a fixed Doxygen) |
| `docs-links` | `check_docs_links.py` |
| `crc-regen` | Committed CRC source matches the pycrc output |

`release.yml` builds the Linux x86_64 tarball, source archive, checksums and
manifest for a `v*` tag. `metrics.yml` publishes size metrics as an artifact
and gates nothing.

## Releasing

The order matters (#18): the version bump pins the examples' `platformio.ini`
to the new version, so their CI lane can only pass once the registry has it,
while the tag must point at that same commit.

1. On a branch, bump the version in `CMakeLists.txt`, `library.json`,
   `library.properties`, `src/crumbs_version.h`, the examples' `lib_deps` and
   any doc that quotes it; date the `[Unreleased]` section. Open the PR; its
   `platformio` lane fails with `UnknownPackageError` — expected.
2. Tag that commit (`git tag -a vX.Y.Z -m "Release vX.Y.Z"`, push the tag).
   `release.yml` builds the GitHub release from it.
3. `pio pkg publish` from the tagged tree; wait until
   `pio pkg show cameronbrooks11/CRUMBS@X.Y.Z` resolves.
4. Re-run the PR's `platformio` lane, then merge with a **merge commit** (the
   repo allows both; releases are the one case that uses it) so
   the tagged SHA stays an ancestor of `main`.
5. Check the release assets and `git merge-base --is-ancestor vX.Y.Z main`.

CRUMBS is not in the Arduino Library Manager; Arduino users install from the
release archive.
