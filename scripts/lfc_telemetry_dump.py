#!/usr/bin/env python3
# Linux Fan Control — SHM telemetry dumper to text file (JSON pretty print)
# (c) 2025 LinuxFanControl contributors
# All comments in English; no placeholders.

import argparse
import json
import os
import sys
from typing import Any, Optional


def shm_name_normalize(path_or_name: str) -> str:
    """Normalize a SHM name/path to a POSIX shm name starting with '/'."""
    if not path_or_name:
        return "/lfc.telemetry"
    base = os.path.basename(path_or_name) if "/" in path_or_name else path_or_name
    return base if base.startswith("/") else "/" + base


def shm_file_from_name(path_or_name: str) -> str:
    """Translate a POSIX shm name to its backing file path in /dev/shm."""
    name = shm_name_normalize(path_or_name)
    return os.path.join("/dev/shm", name.lstrip("/"))


def read_shm_json(shm_file: str) -> Optional[Any]:
    """Read and parse JSON text from the SHM-backed file. Returns parsed object or None."""
    try:
        if not os.path.exists(shm_file):
            return None
        # Read generously and strip trailing NUL padding (if any)
        size = max(131072, os.path.getsize(shm_file) or 0)
        with open(shm_file, "rb") as f:
            buf = f.read(size).rstrip(b"\x00")
        if not buf:
            return None
        return json.loads(buf.decode("utf-8", errors="ignore"))
    except Exception:
        return None


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Dump Linux Fan Control SHM telemetry JSON to a text file (pretty-printed)."
    )
    ap.add_argument(
        "--shm",
        default="lfc.telemetry",
        help="POSIX shm name or path (default: lfc.telemetry)",
    )
    ap.add_argument(
        "--out",
        default="/tmp/lfc_shm.txt",
        help="Output file path (default: /tmp/lfc_shm.txt)",
    )
    ap.add_argument(
        "--indent",
        type=int,
        default=2,
        help="JSON indentation (default: 2)",
    )
    ap.add_argument(
        "--ensure-ascii",
        action="store_true",
        help="Escape non-ASCII characters (default: keep UTF-8)",
    )
    args = ap.parse_args()

    shm_file = shm_file_from_name(args.shm)
    doc = read_shm_json(shm_file)

    if doc is None:
        print(f"[!] Telemetry SHM not found or invalid JSON: {shm_file}", file=sys.stderr)
        print("    Hint: run lfcd or check the SHM name via daemon.json / --shm", file=sys.stderr)
        return 2

    try:
        js = json.dumps(doc, indent=args.indent, ensure_ascii=args.ensure_ascii)
        # Ensure parent directory exists
        out_path = os.path.abspath(args.out)
        parent = os.path.dirname(out_path)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(js)
            f.write("\n")
    except Exception as e:
        print(f"[!] Failed to write output file: {args.out} ({e})", file=sys.stderr)
        return 3

    return 0


if __name__ == "__main__":
    sys.exit(main())
