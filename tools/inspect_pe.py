"""Inspect an x64 PE image without executing it or modifying the input."""
import argparse
import hashlib
import json
from pathlib import Path

import pefile


def inspect(path: Path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    image = pefile.PE(str(path), fast_load=True)
    try:
        image.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"],
        ])
        return {
            "sha256": digest.hexdigest(),
            "machine": hex(image.FILE_HEADER.Machine),
            "entry_rva": hex(image.OPTIONAL_HEADER.AddressOfEntryPoint),
            "image_size": image.OPTIONAL_HEADER.SizeOfImage,
            "sections": [
                {"name": section.Name.rstrip(b"\0").decode("ascii", "replace"),
                 "rva": hex(section.VirtualAddress), "size": section.Misc_VirtualSize}
                for section in image.sections
            ],
            "imports": [
                {"library": entry.dll.decode("ascii", "replace"), "count": len(entry.imports)}
                for entry in getattr(image, "DIRECTORY_ENTRY_IMPORT", [])
            ],
            "runtime_function_count": len(getattr(image, "DIRECTORY_ENTRY_EXCEPTION", [])),
        }
    finally:
        image.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.image), indent=2))
    except (OSError, pefile.PEFormatError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()

