#!/usr/bin/env python3
"""
Bump zovutils version consistently across zov.toml, README.md, src/main.zov,
src/shared/*.zov, and all command modules in src/commands/*.zov.

Usage:
    python3 scripts/bump_version.py <new_version>
"""

import sys
import re
from pathlib import Path

def bump(target_version: str) -> None:
    root = Path(__file__).resolve().parents[1]
    manifest_path = root / "zov.toml"
    manifest_text = manifest_path.read_text(encoding="utf-8")
    match = re.search(r'^version = "([^"]+)"$', manifest_text, re.MULTILINE)
    if not match:
        print("Error: could not find current version in zov.toml", file=sys.stderr)
        sys.exit(1)
    current_version = match.group(1)
    if current_version == target_version:
        print(f"Already at version {target_version}")
        return

    print(f"Bumping zovutils: {current_version} -> {target_version}")

    updated_files = 0
    # Update zov.toml
    new_manifest = manifest_text.replace(f'version = "{current_version}"', f'version = "{target_version}"', 1)
    manifest_path.write_text(new_manifest, encoding="utf-8")
    updated_files += 1

    # Update README.md
    readme_path = root / "README.md"
    if readme_path.exists():
        readme_text = readme_path.read_text(encoding="utf-8")
        new_readme = readme_text.replace(current_version, target_version)
        if new_readme != readme_text:
            readme_path.write_text(new_readme, encoding="utf-8")
            updated_files += 1

    # Update src/**/*.zov
    for p in (root / "src").rglob("*.zov"):
        text = p.read_text(encoding="utf-8")
        if current_version in text:
            p.write_text(text.replace(current_version, target_version), encoding="utf-8")
            updated_files += 1

    print(f"Successfully updated {updated_files} files to {target_version}.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 scripts/bump_version.py <new_version>", file=sys.stderr)
        sys.exit(1)
    bump(sys.argv[1].strip())
