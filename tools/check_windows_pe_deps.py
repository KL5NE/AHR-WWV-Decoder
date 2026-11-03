"""Static Windows PE loader diagnostics and FFmpeg runtime consistency checks.

Models the default SafeDllSearchMode sequence: application directory,
System32, Windows, current directory, then PATH. Does not model all KnownDLLs,
API-set redirects or LOAD_LIBRARY_SEARCH flags. The actual QPluginLoader
integration test and pinned FFmpeg file-hash gate remain authoritative.
"""
from __future__ import annotations
import hashlib
import os
import sys
from pathlib import Path
import pefile

arguments = sys.argv[1:]
reference = None
if "--ffmpeg-reference" in arguments:
    ix = arguments.index("--ffmpeg-reference")
    if ix + 1 >= len(arguments):
        raise SystemExit("Missing directory after --ffmpeg-reference")
    reference = Path(arguments[ix + 1]).resolve()
    del arguments[ix:ix+2]
if not arguments:
    raise SystemExit("Usage: check_windows_pe_deps.py EXE [DLL ...] [--ffmpeg-reference DIR]")
roots = [Path(p).resolve() for p in arguments]
win = Path(os.getenv("WINDIR", r"C:\Windows"))
search = [roots[0].parent, win/"System32", win, Path.cwd()]
search += [Path(s) for s in os.environ.get("PATH","").split(os.pathsep) if s]
resolved = {}
visited = {}
missing = []
unresolved = []
pe_cache = {}
ffmpeg_names = ("avcodec-", "avformat-", "avutil-", "avfilter-", "avdevice-",
                "swresample-", "swscale-", "postproc-")

def lookup(name):
    key = name.lower()
    if key in resolved:
        return resolved[key]
    for folder in search:
        path = folder/name
        if path.is_file():
            resolved[key] = path.resolve()
            return resolved[key]
    return None

def parse(path):
    key = str(path).lower()
    if key not in pe_cache:
        try:
            obj = pefile.PE(str(path), fast_load=True)
            obj.parse_data_directories(directories=[
                pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
                pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
            pe_cache[key] = obj
        except Exception as exc:
            print(f"WARNING cannot inspect {path}: {exc}")
            pe_cache[key] = None
    return pe_cache[key]

def visit(path, chain=()):
    key = str(path).lower()
    if key in visited:
        return
    visited[key] = path
    binary = parse(path)
    if binary is None:
        return
    for entry in getattr(binary, "DIRECTORY_ENTRY_IMPORT", []):
        name = entry.dll.decode("ascii", errors="replace")
        if name.lower().startswith(("api-ms-win-", "ext-ms-")):
            continue
        dep = lookup(name)
        lineage = chain + (path.name,)
        if dep is None:
            missing.append(" -> ".join(lineage + (name,)))
            continue
        imported = parse(dep)
        if imported is not None:
            exports = getattr(getattr(imported,"DIRECTORY_ENTRY_EXPORT",None),"symbols",[])
            names = {sym.name for sym in exports if sym.name is not None}
            ordinals = {sym.ordinal for sym in exports}
            for symbol in entry.imports:
                if symbol.name is not None:
                    ok = symbol.name in names
                    label = symbol.name.decode("ascii", errors="replace")
                else:
                    ok = symbol.ordinal in ordinals
                    label = f"ordinal #{symbol.ordinal}"
                if not ok:
                    unresolved.append(" -> ".join(lineage+(f"{name} [{dep}]",label)))
        visit(dep, lineage)

for root in roots:
    if root.is_file():
        resolved.setdefault(root.name.lower(), root)
        print(f"SCAN {root}")
        visit(root)
    else:
        missing.append(str(root))
print("Resolved Windows DLL paths:")
for path in sorted(visited.values(), key=lambda x:x.name.lower()):
    print(f"  {path.name:35s} {path}")
errors=[]
ffmpeg = [p for p in visited.values() if p.name.lower().startswith(ffmpeg_names)]
if ffmpeg and len({p.parent for p in ffmpeg})>1:
    errors.append("FFmpeg DLLs resolve from multiple folders")
if reference is not None:
    for path in ffmpeg:
        standard = reference/path.name
        if not standard.is_file():
            errors.append(f"Reference missing {standard}")
        elif hashlib.sha256(path.read_bytes()).digest()!=hashlib.sha256(standard.read_bytes()).digest():
            errors.append(f"{path} differs from pinned {standard}")
for entry in missing: print("MISSING DLL: "+entry)
for entry in unresolved: print("MISSING EXPORT: "+entry)
for entry in errors: print("FFMPEG MIX: "+entry)
print(f"{len(visited)} modules; {len(missing)} missing DLLs, {len(unresolved)} missing exports, {len(errors)} FFmpeg issues")
sys.exit(int(bool(missing or unresolved or errors)))
