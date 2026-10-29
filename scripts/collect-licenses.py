"""Collect upstream notices without changing or choosing the project's own license."""
from pathlib import Path
import shutil
import sys

source, destination = map(Path, sys.argv[1:])
for module in ("qtbase", "qtdeclarative"):
    root = source / module
    if not (root / "LICENSES" / "LGPL-3.0-only.txt").is_file():
        raise SystemExit(f"Missing upstream licenses: {root}")
    for path in root.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        name = path.name.lower()
        if ("LICENSES" in path.relative_to(root).parts or "license" in name
                or "licence" in name or "copying" in name or "copyright" in name
                or name == "qt_attribution.json"):
            target = destination / module / path.relative_to(root)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
print("Qt license texts and third-party attribution files collected")
