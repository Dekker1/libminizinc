#!/usr/bin/env python3
"""Package one or more MiniZinc library directories as a single ``.lib.mzn`` file.

The bundle contains the concatenated contents of every ``.mzn`` file in the
library, each preceded by a pragma naming the file that follows::

    /*** @mzn_lib_version 1 ***/
    /*** @replace_file "globals.mzn" ***/
    ...contents of globals.mzn...
    /*** @replace_file "stdlib/stdlib_ite.mzn" ***/
    ...contents of stdlib/stdlib_ite.mzn...

MiniZinc accepts a bundle wherever an include-path library directory is valid.
``@replace_file`` reports locations in the original files, as required by the
standard library. ``@override_file`` (see ``--override``) reports locations in
the bundle, as required by solver libraries.

Several directories can be given, and are flattened into one library the way
the include path would resolve them: the first directory that has a file wins,
later ones only supply what the earlier ones do not contain. A layered
``@replace_file`` pragma carries a second name, the file's path relative to the
directory holding the bundle, so that locations still point at the library the
file actually came from::

    /*** @replace_file "helper.mzn" "mylib/helper.mzn" ***/
"""

import argparse
import base64
import re
import sys
import zlib
from pathlib import Path

LIB_VERSION = 1

VERSION_PRAGMA = '/*** @mzn_lib_version {} ***/'
FILE_PRAGMA = '/*** @{} "{}" ***/'
FILE_PRAGMA_LAYERED = '/*** @{} "{}" "{}" ***/'

# Any line matching this in a source file would be indistinguishable from a
# pragma once bundled, so it is rejected rather than silently mangled.
PRAGMA_RE = re.compile(r'^/\*\*\* @(replace_file|override_file|mzn_lib_version)\b')


def collect(libdirs):
    """Return sorted ``(key, libdir, path)`` triples for the .mzn files.

    Layered like the include path: for a file present in more than one
    directory, the first directory given wins.
    """
    found = {}
    for libdir in libdirs:
        for key, path in collect_one(libdir):
            found.setdefault(key, (libdir, path))
    return [(key, libdir, path) for key, (libdir, path) in sorted(found.items())]


def collect_one(libdir: Path):
    """Return ``(key, path)`` pairs for each .mzn file under ``libdir``."""
    files = []
    for path in libdir.rglob("*.mzn"):
        if path.is_file():
            files.append((path.relative_to(libdir).as_posix(), path))
    files.sort()
    return files


def read_checked(key: str, path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    for lineno, line in enumerate(text.splitlines(), start=1):
        if PRAGMA_RE.match(line):
            raise SystemExit(
                f"{path}:{lineno}: line looks like a bundle pragma and cannot "
                f"be bundled: {line!r}"
            )
    if text and not text.endswith("\n"):
        text += "\n"
    return text


def build(libdirs, override: bool) -> str:
    pragma = "override_file" if override else "replace_file"
    # With one library there is nothing to disambiguate: the reader derives the
    # origin from the bundle's own name.
    layered = len(libdirs) > 1 and not override
    out = [VERSION_PRAGMA.format(LIB_VERSION), "\n"]
    for key, libdir, path in collect(libdirs):
        if layered:
            out.append(FILE_PRAGMA_LAYERED.format(pragma, key, f"{libdir.name}/{key}"))
        else:
            out.append(FILE_PRAGMA.format(pragma, key))
        out.append("\n")
        out.append(read_checked(key, path))
    return "".join(out)


def compress(text: str) -> bytes:
    """Encode as the compressed-model format libminizinc already understands."""
    return b"@" + base64.b64encode(zlib.compress(text.encode("utf-8"), 9))


def strip_separator(libdir: Path) -> Path:
    while libdir.name == "":
        libdir = libdir.parent
    return libdir


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument(
        "libdir",
        type=Path,
        nargs="+",
        help="library directories to bundle, highest priority first",
    )
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        help="output file (default: <libdir>.lib.mzn; required for several "
        "directories)",
    )
    p.add_argument(
        "--override",
        action="store_true",
        help="emit @override_file pragmas (for solver libraries): locations "
        "point into the bundle instead of the original files",
    )
    compression = p.add_mutually_exclusive_group()
    compression.add_argument(
        "--compress",
        action="store_true",
        default=None,
        help="deflate and base64-encode the bundle (the default unless "
        "--override is given)",
    )
    compression.add_argument(
        "--no-compress",
        dest="compress",
        action="store_false",
        help="write the bundle as plain text",
    )
    args = p.parse_args()

    libdirs = [strip_separator(d) for d in args.libdir]
    for libdir in libdirs:
        if not libdir.is_dir():
            raise SystemExit(f"{libdir}: not a directory")

    output = args.output
    if output is None:
        if len(libdirs) > 1:
            raise SystemExit("-o is required when bundling several directories")
        output = libdirs[0].with_name(libdirs[0].name + ".lib.mzn")

    # A solver library is usually small and read once; the standard library is
    # neither, so it is compressed unless asked otherwise.
    compressed = args.compress if args.compress is not None else not args.override

    text = build(libdirs, args.override)
    if compressed:
        output.write_bytes(compress(text))
    else:
        output.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
