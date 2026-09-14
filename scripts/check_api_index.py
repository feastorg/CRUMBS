#!/usr/bin/env python3
"""Fail if a public symbol is missing from docs/api-reference.md.

Reads the Doxygen XML that scripts/doccheck.sh produces (docs/doxygen_out/xml)
and requires every function, macro, typedef, struct and enum value from the
public headers to appear by name in the API reference. Exit 1 if any is absent.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XML_DIR = ROOT / "docs" / "doxygen_out" / "xml"
DOC = ROOT / "docs" / "api-reference.md"


def public_symbols():
    names = set()
    for xml in XML_DIR.glob("*.xml"):
        if xml.name == "index.xml":
            continue
        for cd in ET.parse(xml).getroot().iter("compounddef"):
            kind = cd.get("kind")
            if kind in ("struct", "union"):
                name = cd.findtext("compoundname")
                # A struct tag documented through its typedef counts as covered.
                names.add(re.sub(r"_s$", "_t", name))
            elif kind == "file":
                for m in cd.iter("memberdef"):
                    if m.get("kind") in ("function", "define", "typedef", "enum"):
                        names.add(m.findtext("name"))
                    if m.get("kind") == "enum":
                        names.update(ev.findtext("name") for ev in m.iter("enumvalue"))
    return names


def main() -> int:
    if not XML_DIR.is_dir():
        print(f"{XML_DIR} not found: run scripts/doccheck.sh first", file=sys.stderr)
        return 2
    text = DOC.read_text(encoding="utf-8")
    names = public_symbols()
    if not names:
        print(f"no public symbols found in {XML_DIR}: is the Doxygen XML stale or empty?", file=sys.stderr)
        return 2
    missing = sorted(n for n in names if not re.search(rf"\b{re.escape(n)}\b", text))
    for n in missing:
        print(f"{DOC.relative_to(ROOT)}: public symbol not indexed: {n}")
    if missing:
        print(f"{len(missing)} of {len(names)} public symbols missing")
        return 1
    print(f"api index: all {len(names)} public symbols are in {DOC.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
