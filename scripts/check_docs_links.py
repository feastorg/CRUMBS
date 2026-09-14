#!/usr/bin/env python3
"""Fail on broken relative links and anchors in the repository's Markdown.

Checks every tracked *.md file: a relative link target must exist, and a
#fragment must match a heading in the target file the way GitHub slugs it.
External (scheme) links are not fetched. Exit 1 if anything is broken.
"""
import re
import subprocess
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LINK_RE = re.compile(r"(?<!\\)!?\[(?:[^\[\]]|!\[[^\]]*\]\([^)]*\))*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
REF_DEF_RE = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*(\S+)", re.M)
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$", re.M)
FENCE_RE = re.compile(r"^[ \t]*(`{3,}|~{3,}).*?^[ \t]*\1[ \t]*$", re.M | re.S)
HTML_ANCHOR_RE = re.compile(r"<a\s+(?:name|id)=\"([^\"]+)\"")


def github_slug(text: str) -> str:
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = text.strip().lower()
    # GitHub keeps letters, marks, digits, connectors, hyphens and spaces.
    kept = (c for c in text if c in "- " or unicodedata.category(c)[0] in "LM" or unicodedata.category(c) in ("Nd", "Nl", "Pc"))
    return "".join(kept).replace(" ", "-")


def anchors_of(path: Path) -> set:
    body = FENCE_RE.sub("", path.read_text(encoding="utf-8"))
    seen, out = {}, set()
    for m in HEADING_RE.finditer(body):
        slug = github_slug(m.group(2))
        n = seen.get(slug, 0)
        seen[slug] = n + 1
        out.add(slug if n == 0 else f"{slug}-{n}")
    out.update(HTML_ANCHOR_RE.findall(body))
    return out


def tracked_markdown():
    files = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "*.md", "**/*.md"], text=True).split()
    return sorted({ROOT / f for f in files})


def main() -> int:
    anchor_cache = {}
    broken = []
    checked = 0
    for md in tracked_markdown():
        text = FENCE_RE.sub("", md.read_text(encoding="utf-8"))
        targets = LINK_RE.findall(text) + REF_DEF_RE.findall(text)
        for raw in targets:
            if re.match(r"^[a-z][a-z0-9+.-]*:", raw) or raw.startswith("//"):
                continue
            checked += 1
            target, _, frag = raw.partition("#")
            dest = (md.parent / target).resolve() if target else md
            if not dest.exists():
                broken.append(f"{md.relative_to(ROOT)}: missing target {raw}")
                continue
            if frag and dest.suffix == ".md":
                if dest not in anchor_cache:
                    anchor_cache[dest] = anchors_of(dest)
                if frag.lower() not in anchor_cache[dest]:
                    broken.append(f"{md.relative_to(ROOT)}: no anchor #{frag} in {dest.relative_to(ROOT)}")
    for line in broken:
        print(line)
    print(f"{len(broken)} broken link(s) out of {checked} checked" if broken else f"docs links: {checked} relative links and anchors resolve")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
