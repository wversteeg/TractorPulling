#!/usr/bin/env python3
"""Picks the source files clang-tidy must check for a change.

    scripts/tidy_files.py <build dir> <scope regex> [changed-files list]

Prints, one per line as an anchored regex for run-clang-tidy, every source
file in the build's compile_commands.json whose path relative to the
repository root matches the scope regex (e.g. '^(core|tests)/') and which is, or includes, a file in the list (paths relative to the
repository root, as `git diff --name-only` prints them). The includes come
from clang-scan-deps, which reads the compile commands without building.

Without a list, or when the list holds a file that changes how everything
is checked or compiled (.clang-tidy, a CMakeLists.txt, cmake/, the presets,
the CI workflow, this script), it prints every source file in scope: the
full check, as on a push to a branch.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DEPS = os.environ.get("CLANG_SCAN_DEPS", "clang-scan-deps-18")
CHECK_EVERYTHING = re.compile(
    r"^(\.clang-tidy|CMakePresets\.json|cmake/.*|\.github/workflows/ci\.yml"
    r"|scripts/tidy_files\.py|(.*/)?CMakeLists\.txt)$"
)


def sources_in_scope(build_dir, scope):
    with open(build_dir / "compile_commands.json", encoding="utf-8") as database:
        commands = json.load(database)
    in_scope = []
    for command in commands:
        source = Path(command["directory"], command["file"]).resolve()
        if source.is_relative_to(ROOT) and re.search(scope, str(source.relative_to(ROOT))):
            in_scope.append((source, command))
    return in_scope


def includes_by_source(in_scope):
    """Each source's own path and every file it includes, from clang-scan-deps."""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as database:
        json.dump([command for _, command in in_scope], database)
    try:
        rules = subprocess.run(
            [SCAN_DEPS, "-compilation-database", database.name, "-format=make",
             "-j", str(os.cpu_count() or 1)],
            check=True, capture_output=True, text=True,
        ).stdout
    finally:
        os.unlink(database.name)
    # One make rule per source: "target.o: source.cpp header.hpp ... \".
    deps = {}
    for rule in re.split(r"\n(?=\S)", rules.replace("\\\n", " ")):
        _, _, prerequisites = rule.partition(": ")
        paths = [Path(p).resolve() for p in prerequisites.split()]
        if paths:
            deps[paths[0]] = set(paths)
    return deps


def main(argv):
    if len(argv) not in (3, 4):
        print(__doc__, file=sys.stderr)
        return 2
    build_dir, scope = Path(argv[1]), argv[2]
    in_scope = sources_in_scope(build_dir, scope)
    selected = [source for source, _ in in_scope]

    if len(argv) == 4:
        with open(argv[3], encoding="utf-8") as listing:
            changed = [line.strip() for line in listing if line.strip()]
        if not any(CHECK_EVERYTHING.match(path) for path in changed):
            changed_paths = {(ROOT / path).resolve() for path in changed}
            deps = includes_by_source(in_scope)
            # A source clang-scan-deps could not read is checked anyway.
            selected = [s for s in selected if s not in deps or deps[s] & changed_paths]

    for source in sorted(set(selected)):
        print(f"^{re.escape(str(source))}$")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
