#!/usr/bin/env python3
"""
Bump zovutils version consistently across zov.toml, README.md, src/main.zov,
src/shared/*.zov, and all command modules in src/commands/*.zov.

Autonomous bump rule:
    0.*.0 (minor milestone): new tool, platform compatibility, major subsystem/feature
    0.0.* (patch release): bug fixes, parity adjustments, performance optimizations

Usage:
    python3 scripts/bump_version.py [new_version | --auto | auto]
"""

from __future__ import annotations

import sys
import re
import subprocess
from pathlib import Path


def determine_next_version(current_version: str, root: Path) -> str:
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)$", current_version)
    if not m:
        raise ValueError(f"Version {current_version} is not in major.minor.patch format")
    major, minor, patch = int(m.group(1)), int(m.group(2)), int(m.group(3))

    # Find the last bump commit
    cmd = ["git", "log", "--grep=bump zovutils", "-n", "1", "--format=%H"]
    res = subprocess.run(cmd, cwd=root, capture_output=True, text=True, check=False)
    last_bump = res.stdout.strip()

    if not last_bump:
        cmd2 = ["git", "log", "-n", "1", "--skip=1", "--format=%H", "zov.toml"]
        res2 = subprocess.run(cmd2, cwd=root, capture_output=True, text=True, check=False)
        last_bump = res2.stdout.strip()

    log_range = f"{last_bump}..HEAD" if last_bump else "-n 20"
    cmd_log = ["git", "log", log_range, "--oneline"]
    res_log = subprocess.run(cmd_log, cwd=root, capture_output=True, text=True, check=False)
    commits = [line.strip() for line in res_log.stdout.strip().splitlines() if line.strip()]

    is_minor = False
    minor_triggers = []
    platform_keywords = ["platform", "windows", "darwin", "linux", "bsd", "compatibility", "static"]

    for c in commits:
        c_lower = c.lower()
        if "feat:" in c_lower or "feat(" in c_lower:
            is_minor = True
            minor_triggers.append(c)
        elif not c_lower.startswith("fix:") and not c_lower.startswith("chore:") and any(k in c_lower for k in platform_keywords):
            is_minor = True
            minor_triggers.append(c)

    if is_minor:
        next_ver = f"{major}.{minor + 1}.0"
        print(f"Autonomous Version Analysis: Minor update detected ({len(minor_triggers)} feature/platform triggers):")
        for t in minor_triggers:
            print(f"  • {t}")
    else:
        next_ver = f"{major}.{minor}.{patch + 1}"
        print(f"Autonomous Version Analysis: Patch update detected ({len(commits)} fixes/optimizations/maintenance)")

    return next_ver


def bump(target_version: str | None = None) -> str:
    root = Path(__file__).resolve().parents[1]
    manifest_path = root / "zov.toml"
    manifest_text = manifest_path.read_text(encoding="utf-8")
    match = re.search(r'^version = "([^"]+)"$', manifest_text, re.MULTILINE)
    if not match:
        print("Error: could not find current version in zov.toml", file=sys.stderr)
        sys.exit(1)
    current_version = match.group(1)

    if not target_version or target_version in ("auto", "--auto"):
        target_version = determine_next_version(current_version, root)

    if current_version == target_version:
        print(f"Already at version {target_version}")
        return current_version

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
    return target_version


if __name__ == "__main__":
    arg = sys.argv[1].strip() if len(sys.argv) > 1 else "--auto"
    bump(arg)
