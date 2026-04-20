# PlatformIO Publishing

This project includes a PlatformIO library manifest (`library.json`) and PlatformIO examples in `examples/core_usage/platformio/` and `examples/handlers_usage/platformio/`. Follow the quick steps below to publish and maintain the package on the PlatformIO Registry.

## Minimum checklist before publishing

- Ensure `library.json` exists in the repository root and contains accurate fields: `name`, `version`, `description`, `authors`, `license`, `frameworks`, `platforms`, `repository`, and `examples`.
- Provide `include/` headers and `src/` sources (or an equivalent layout) so PlatformIO can detect the library.

## Publish to PlatformIO Registry (CLI)

1. Install PlatformIO CLI:

   `pip install -U platformio`

2. (Optional) Create or log in to your PlatformIO account.

   `pio account login`

3. Publish from your repo checkout (user must have permission):

   `pio pkg publish`

**Notes**:

- PlatformIO uses `library.json` as the manifest. The CLI will guide you for required fields.
- Every time the library source changes you should increment the `version` in `library.json` and re-run `pio pkg publish`.

## Alternative: publish via the web UI

- You can also publish via the PlatformIO Registry web site: [PlatformIO Registry](https://platformio.org/lib) — the same `library.json` manifest is used.

## Keeping the registry package up to date

- Bump the `version` field in `library.json` for every published release.
- Optionally tag your release (e.g., `git tag v0.7.1` and push tags). This helps users and CI track versions.
- Consider adding automated CI release steps (for example a GitHub Actions workflow) that call `pio pkg publish` when a new release tag is pushed. That requires storing a PlatformIO token/credentials in GitHub secrets.

## Local development & CI fallback

- While publishing the library is the recommended long-term solution, the examples should also work from the repository directly. There are two ways to make examples build before the library is published:
  - lib_deps: set `lib_deps = https://github.com/feastorg/CRUMBS.git` in the example's `[env:...]` section — PlatformIO will fetch the library from the repo.
  - lib_extra_dirs: during local development you can use `lib_extra_dirs = ${PROJECT_DIR}/../../..` (in the `[platformio]` section) so the example resolves the library from your monorepo.

## Recommended workflow for CI

- Publish the library to the registry and reference the published library in `platformio.ini` via `lib_deps = feastorg/CRUMBS` or a versioned release identifier.
- For quick CI progress before publishing, referencing the Git URL (`lib_deps = https://github.com/feastorg/CRUMBS.git`) works and will let CI build examples.

## Quick troubleshooting

- If PlatformIO complains about missing headers (for example `crumbs_arduino.h`), confirm the library provides `include/crumbs_arduino.h` and that `lib_deps` or `lib_extra_dirs` point to the correct location.

## Further reading

- PlatformIO docs: [PlatformIO library manager docs](https://docs.platformio.org/en/latest/librarymanager/index.html)
- Publishing guide: [Publishing libraries](https://docs.platformio.org/en/latest/librarymanager/creating.html#publishing)
