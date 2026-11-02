"""Keep immutable ELF/MAP snapshots for panic symbolization; never build or flash."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil


def digest(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--map", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    elf = args.elf.resolve()
    with elf.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            raise ValueError(f"Not a complete ELF file: {elf}")
    sha = digest(elf)
    destination = args.output.resolve() / sha
    destination.mkdir(parents=True, exist_ok=True)
    saved_elf = destination / elf.name
    if saved_elf.exists():
        if digest(saved_elf) != sha:
            raise ValueError(f"Archive contents disagree with SHA directory: {saved_elf}")
    else:
        temporary = destination / (elf.name + ".copying")
        shutil.copy2(elf, temporary)
        if digest(temporary) != sha or digest(elf) != sha:
            raise ValueError("ELF changed while archiving; do not use this snapshot")
        temporary.replace(saved_elf)
    saved_map = None
    if args.map and args.map.is_file():
        saved_map = destination / args.map.name
        if not saved_map.exists():
            shutil.copy2(args.map, saved_map)
    manifest = destination / "manifest.json"
    if not manifest.exists():
        manifest.write_text(json.dumps({
            "elf_sha256": sha,
            "elf": saved_elf.name,
            "map": saved_map.name if saved_map else None,
            "source_elf": str(elf),
        }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Firmware symbols archived: {saved_elf}")


if __name__ == "__main__":
    main()
