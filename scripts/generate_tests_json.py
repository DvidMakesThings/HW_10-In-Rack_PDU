# scripts/generate_tests_json.py
"""
Generate / update Test/tests.json by scanning the Test/ directory.

Usage (run from repo root):
    python scripts/generate_tests_json.py

Notes:
- Keeps existing metadata (title, note) when href matches.
- Sets file size (bytes) automatically.
- Date is taken from:
    1) existing entry, else
    2) from filename prefix YYYY-MM-DD_*, else
    3) last modification time.
"""

from __future__ import annotations
import json
import os
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Any, List

RE_DATE_PREFIX = re.compile(r"^(\d{4}-\d{2}-\d{2})[_-]")

REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_DIR = REPO_ROOT / "tests"
INDEX_PATH = TEST_DIR / "tests.json"


def iso_date(dt: datetime) -> str:
    return dt.strftime("%Y-%m-%d")


def load_existing_index() -> Dict[str, Any]:
    if INDEX_PATH.exists():
        try:
            with INDEX_PATH.open("r", encoding="utf-8") as f:
                data = json.load(f)
                if isinstance(data, dict) and "reports" in data and isinstance(data["reports"], list):
                    return data
        except Exception:
            pass
    return {"updated": None, "reports": []}


def index_by_href(reports: List[Dict[str, Any]]) -> Dict[str, Dict[str, Any]]:
    return {str(r.get("href", "")).strip(): r for r in reports if isinstance(r, dict) and r.get("href")}


def infer_type(path: Path) -> str:
    ext = path.suffix.lower().lstrip(".")
    if ext in ("html", "htm"):
        return "html"
    if ext == "pdf":
        return "pdf"
    return "file"


def infer_date_from_name(name: str) -> str | None:
    m = RE_DATE_PREFIX.match(name)
    if m:
        return m.group(1)
    return None


def main() -> None:
    if not TEST_DIR.exists():
        raise SystemExit(f"Missing directory: {TEST_DIR}")

    existing = load_existing_index()
    by_href = index_by_href(existing["reports"])

    new_reports: List[Dict[str, Any]] = []

    for path in sorted(TEST_DIR.rglob("*")):
        if path.is_dir():
            continue
        rel = path.relative_to(TEST_DIR).as_posix()

        # Only index common report formats
        if path.suffix.lower() not in (".html", ".htm", ".pdf"):
            continue

        size = path.stat().st_size
        prev = by_href.get(rel, {})

        # Title: keep if present; else from previous; else from basename
        title = prev.get("title") or path.name

        # Date priority: existing -> filename prefix -> mtime
        date_str = prev.get("date")
        if not date_str:
            date_str = infer_date_from_name(path.name)
        if not date_str:
            dt = datetime.fromtimestamp(path.stat().st_mtime, tz=timezone.utc)
            date_str = iso_date(dt)

        entry = {
            "href": rel,
            "title": title,
            "date": date_str,
            "type": prev.get("type") or infer_type(path),
            "size": size
        }
        # Keep optional note if it existed
        if "note" in prev:
            entry["note"] = prev["note"]

        new_reports.append(entry)

    # Sort by date desc then href
    def sort_key(r: Dict[str, Any]):
        try:
            return (datetime.strptime(r.get("date", "1970-01-01"), "%Y-%m-%d"), r.get("href", ""))
        except Exception:
            return (datetime(1970, 1, 1), r.get("href", ""))

    new_reports.sort(key=sort_key, reverse=True)

    out = {
        "updated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "reports": new_reports
    }

    INDEX_PATH.parent.mkdir(parents=True, exist_ok=True)
    with INDEX_PATH.open("w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)

    print(f"Wrote {INDEX_PATH} with {len(new_reports)} reports.")


if __name__ == "__main__":
    main()
