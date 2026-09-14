#!/usr/bin/env python3
"""Generate CRC-8 C99 variants and stage them into the repo src/crc directory.

This consolidates the previous two-step flow (generate C99 then run a separate
staging script) into a single, repeatable script. The generator will:

- Invoke pycrc to generate C99 headers and sources into dist/crc/c99
- Optionally copy (stage) selected variations into src/crc so the library
  builds against a specific generated algorithm variant.

Defaults: The common project uses the nibble (table-driven, 4-bit index)
variant - staging defaults to ['nibble'] to match current project usage.

CI (the crc-regen job) regenerates with pycrc pinned to the version named in
the committed banner (see "by pycrc v..." in src/crc/crc8_nibble.h) and diffs
the result against src/crc. Regenerate with that same version, or the banner
line alone will fail the check.

Usage examples:
  # generate c99 variants (default set) and stage the nibble variant
  python scripts/generate_crc8.py

  # generate only nibble and byte, but do not stage
  python scripts/generate_crc8.py --algos nibble,byte --no-stage

  # generate all variants and stage all into src/crc
  python scripts/generate_crc8.py --algos bit,nibble,nibblem,byte
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Iterable

SCRIPT_DIR = Path(__file__).parent
REPO_ROOT = SCRIPT_DIR.parent
C99_DIR = REPO_ROOT / "dist" / "crc" / "c99"
C99_DIR.mkdir(parents=True, exist_ok=True)

SRC_CRC_DIR = REPO_ROOT / "src" / "crc"

# Available algorithm tokens and flags passed through to `pycrc`
MODEL = "crc-8"
ALL_ALGOS = ["bit", "nibble", "nibblem", "byte"]
ALGO_FLAGS = {
    "bit": ["--algorithm", "bit-by-bit-fast"],
    "nibble": ["--algorithm", "table-driven", "--table-idx-width", "4"],
    "nibblem": ["--algorithm", "table-driven", "--table-idx-width", "4"],
    "byte": ["--algorithm", "table-driven", "--table-idx-width", "8"],
}


def run(cmd: Iterable[str]) -> None:
    print(">>", " ".join(cmd))
    subprocess.run(list(cmd), check=True)


def generate_c99_variants(algos: Iterable[str]) -> None:
    """Invoke pycrc to generate .h / .c for each requested algorithm token."""
    for algo in algos:
        if algo not in ALGO_FLAGS:
            raise SystemExit(f"Unknown algorithm token: {algo}")

    for algo in algos:
        fileroot = f"crc8_{algo}"
        flags = ALGO_FLAGS[algo]

        h_out = C99_DIR / f"{fileroot}.h"
        c_out = C99_DIR / f"{fileroot}.c"

        # Generate header
        run(
            [
                sys.executable,
                "-m",
                "pycrc",
                "--model",
                MODEL,
                "--std",
                "C99",
                *flags,
                "--generate",
                "h",
                "-o",
                str(h_out),
            ]
        )

        # Generate source
        run(
            [
                sys.executable,
                "-m",
                "pycrc",
                "--model",
                MODEL,
                "--std",
                "C99",
                *flags,
                "--generate",
                "c",
                "-o",
                str(c_out),
            ]
        )


def stage_variants(algos: Iterable[str]) -> int:
    """Copy the generated c99 outputs from dist/crc/c99 -> src/crc.

    Default behavior is to stage header + source matching each fileroot.
    Returns 0 on success, non-zero on failure.
    """
    copied = 0
    SRC_CRC_DIR.mkdir(parents=True, exist_ok=True)

    for algo in algos:
        prefix = f"crc8_{algo}"
        hsrc = C99_DIR / (prefix + ".h")
        csrc = C99_DIR / (prefix + ".c")
        if not hsrc.exists() or not csrc.exists():
            print(f"Missing generated file(s) for {algo}: {hsrc}, {csrc}")
            return 1

        # copy into src/crc (replace existing files)
        dst_h = SRC_CRC_DIR / hsrc.name
        dst_c = SRC_CRC_DIR / csrc.name
        dst_h.write_text(hsrc.read_text())
        dst_c.write_text(csrc.read_text())
        print(f"Staged {hsrc.name} -> {dst_h}")
        print(f"Staged {csrc.name} -> {dst_c}")
        copied += 2

    print(f"Staged {copied} files into {SRC_CRC_DIR}")
    return 0


def parse_algos_arg(val: str | None) -> list[str]:
    if not val:
        return ["nibble"]  # default to nibble to match current project usage
    tokens = [t.strip() for t in val.split(",") if t.strip()]
    if not tokens:
        return ["nibble"]
    for t in tokens:
        if t not in ALL_ALGOS:
            raise SystemExit(f"Unknown algo token in --algos: {t}")
    return tokens


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(
        description="Generate CRC-8 C99 variants and optionally stage them into src/crc"
    )
    parser.add_argument(
        "--algos",
        help=(
            "Comma-separated list of crc8 variations to generate and stage "
            "(bit,nibble,nibblem,byte). Defaults to 'nibble' which is the "
            "variant used by the library by default."
        ),
    )
    parser.add_argument(
        "--no-stage",
        action="store_true",
        help="Generate outputs under dist/crc/c99 but do not stage into src/crc",
    )

    args = parser.parse_args()

    algos = parse_algos_arg(args.algos)

    print("Generating C99 CRC-8 variants into:", C99_DIR)
    generate_c99_variants(algos)

    if args.no_stage:
        print("Skipping staging (no changes to src/crc)")
        return

    rc = stage_variants(algos)
    if rc != 0:
        raise SystemExit("Staging failed; check generated files in dist/crc/c99")


if __name__ == "__main__":
    main()
