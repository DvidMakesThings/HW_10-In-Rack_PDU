#!/usr/bin/env python3
from pathlib import Path
from collections import defaultdict

EXTS = {".c", ".h", ".html"}
SKIP_DIRS = {
    ".git",
    "build",
    ".vscode",
    ".idea",
    "cmake-build-debug",
    "cmake-build-release",
}

ROOT = Path("src")

def should_skip(path: Path) -> bool:
    return any(part in SKIP_DIRS for part in path.parts)

def count_lines(file: Path) -> int:
    try:
        with file.open("r", errors="ignore") as f:
            return sum(1 for _ in f)
    except OSError:
        return 0

def main() -> None:
    if not ROOT.exists():
        raise SystemExit("src folder not found. Run from repo root.")

    files = []
    folder_totals = defaultdict(int)
    folder_files = defaultdict(list)

    for f in ROOT.rglob("*"):
        if f.is_file() and f.suffix in EXTS and not should_skip(f):
            loc = count_lines(f)
            files.append((loc, f))

            folder = f.parent.relative_to(ROOT)
            folder_totals[folder] += loc
            folder_files[folder].append((loc, f))

    total_lines = sum(loc for loc, _ in files)

    print(f"\nFiles counted: {len(files)}")
    print(f"Total lines: {total_lines}")

    print("\n=== TOP 10 FILES ===")
    for loc, f in sorted(files, reverse=True)[:10]:
        print(f"{loc:6d}  {f}")

    print("\n=== LINES BY FOLDER ===")
    for folder, loc in sorted(folder_totals.items(), key=lambda x: x[1], reverse=True):
        print(f"{loc:6d}  {folder}")

    print("\n=== FILES PER FOLDER ===")
    for folder, flist in sorted(folder_files.items(), key=lambda x: sum(v[0] for v in x[1]), reverse=True):
        print(f"\n[{folder}]")
        for loc, f in sorted(flist, reverse=True):
            print(f"  {loc:6d}  {f.name}")

if __name__ == "__main__":
    main()
