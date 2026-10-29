"""Small offline check of required documents, local Markdown links and text hygiene."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
required = ("README.md", "AGENTS.md", "SPEC.md", "ARCHITECTURE.md", "ROADMAP.md",
            "DECISIONS.md", "STATUS.md", "tasks/000-repository-setup.md")
errors = []
for name in required:
    if not (root / name).is_file():
        errors.append(f"Missing document: {name}")
files = subprocess.check_output(["git", "ls-files", "--cached", "--others",
                                 "--exclude-standard"], cwd=root, text=True).splitlines()
for name in files:
    path = root / name
    if path.suffix.lower() in {".mp3", ".flac", ".aac", ".alac", ".m4a", ".mp4"}:
        errors.append(f"Unexpected media: {name}")
    if path.suffix != ".md":
        continue
    content = path.read_text(encoding="utf-8")
    if "\x00" in content or "\ufffd" in content:
        errors.append(f"Invalid text: {name}")
    for link in re.findall(r"\]\(([^)]+)\)", content):
        target = link.split("#", 1)[0]
        if not target or re.match(r"[a-zA-Z][a-zA-Z0-9+.-]*:", target):
            continue
        if not (path.parent / target).is_file():
            errors.append(f"Broken local link in {name}: {link}")
if errors:
    raise SystemExit("\n".join(errors))
print(f"Repository documentation check passed ({len(files)} files); not a secret audit")
