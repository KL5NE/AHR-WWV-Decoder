"""Check recursively imported Windows PE DLLs and print missing-dependency chains.

Diagnostic only: Windows API-set imports and delay imports are not always
resolvable using ordinary PATH searches. Always keep the actual launch test.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import pefile

roots = [Path(p).resolve() for p in sys.argv[1:]]
search_dirs = [Path.cwd(), Path.cwd() / "plugins"]
search_dirs += [Path(p) for p in os.environ.get("PATH", "").split(os.pathsep) if p]
search_dirs += [Path(os.environ.get("WINDIR", r"C:\Windows")) / "System32"]
seen = set()
missing = set()


def resolve(name: str) -> Path | None:
    for folder in search_dirs:
        candidate = folder / name
        if candidate.is_file():
            return candidate
    return None


def visit(path: Path, chain: tuple[str, ...]) -> None:
    name = path.name.lower()
    if name in seen:
        return
    seen.add(name)
    try:
        binary = pefile.PE(str(path), fast_load=True)
        binary.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]]
        )
        imports = getattr(binary, "DIRECTORY_ENTRY_IMPORT", [])
    except Exception as exc:
        print(f"WARNING cannot parse {path}: {exc}", flush=True)
        return
    for imported in imports:
        dep = imported.dll.decode("ascii", errors="replace")
        dep_lower = dep.lower()
        if dep_lower.startswith(("api-ms-win-", "ext-ms-")):
            continue
        located = resolve(dep)
        if located is None:
            key = (chain + (path.name, dep))
            if key not in missing:
                missing.add(key)
                print("MISSING: " + " -> ".join(key), flush=True)
        else:
            visit(located, chain + (path.name,))


for root in roots:
    if root.is_file():
        print(f"SCAN {root}", flush=True)
        visit(root, ())
    else:
        print(f"ROOT MISSING {root}", flush=True)
print(f"Scanned {len(seen)} unique module names; {len(missing)} unresolved import chains.", flush=True)
# This is informative: avoid failing CI before the real launch test.
