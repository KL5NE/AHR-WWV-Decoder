"""Diagnose recursively imported Windows DLLs AND unresolved imported procedures.

Intended for SDRangel 8.0.0/Qt 6 Windows CI. Checks names and ordinals exported
by files found on the test process's PATH, identifying loader error 0xC0000139.
This does not replace a real QPluginLoader integration test.
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

visited: set[str] = set()
missing_files: set[tuple[str, ...]] = set()
missing_exports: set[tuple[str, ...]] = set()
pe_cache: dict[str, pefile.PE | None] = {}
export_cache: dict[str, tuple[set[bytes], set[int]] | None] = {}


def resolve(name: str) -> Path | None:
    for folder in search_dirs:
        candidate = folder / name
        if candidate.is_file():
            return candidate.resolve()
    return None


def load(path: Path) -> pefile.PE | None:
    key = str(path).lower()
    if key not in pe_cache:
        try:
            binary = pefile.PE(str(path), fast_load=True)
            binary.parse_data_directories(directories=[
                pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
                pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"],
            ])
            pe_cache[key] = binary
        except Exception as exc:
            print(f"WARNING: cannot inspect {path}: {exc}", flush=True)
            pe_cache[key] = None
    return pe_cache[key]


def exports(path: Path) -> tuple[set[bytes], set[int]] | None:
    key = str(path).lower()
    if key not in export_cache:
        binary = load(path)
        if binary is None:
            export_cache[key] = None
        else:
            symbols = getattr(getattr(binary, "DIRECTORY_ENTRY_EXPORT", None), "symbols", [])
            export_cache[key] = (
                {sym.name for sym in symbols if sym.name is not None},
                {sym.ordinal for sym in symbols},
            )
    return export_cache[key]


def visit(path: Path, chain: tuple[str, ...]) -> None:
    key = str(path).lower()
    if key in visited:
        return
    visited.add(key)
    binary = load(path)
    if binary is None:
        return
    for imported in getattr(binary, "DIRECTORY_ENTRY_IMPORT", []):
        dependency = imported.dll.decode("ascii", errors="replace")
        lower = dependency.lower()
        if lower.startswith(("api-ms-win-", "ext-ms-")):
            continue
        located = resolve(dependency)
        prefix = chain + (path.name, dependency)
        if located is None:
            if prefix not in missing_files:
                missing_files.add(prefix)
                print("MISSING DLL: " + " -> ".join(prefix), flush=True)
            continue
        available = exports(located)
        if available is not None:
            names, ordinals = available
            for entry in imported.imports:
                if entry.name is not None:
                    symbol = entry.name.decode("utf-8", errors="replace")
                    available_here = entry.name in names
                else:
                    symbol = f"ordinal #{entry.ordinal}"
                    available_here = entry.ordinal in ordinals
                if not available_here:
                    record = prefix + (symbol,)
                    if record not in missing_exports:
                        missing_exports.add(record)
                        print("MISSING EXPORT: " + " -> ".join(record), flush=True)
        visit(located, chain + (path.name,))


for root in roots:
    if root.is_file():
        print(f"SCAN {root}", flush=True)
        visit(root, ())
    else:
        print(f"ROOT MISSING {root}", flush=True)

print(
    f"Inspected {len(visited)} PE modules; "
    f"{len(missing_files)} missing DLL chains, "
    f"{len(missing_exports)} unresolved imported procedures.",
    flush=True,
)
# Diagnostic only: do not interrupt integration tests with a simulated result.
