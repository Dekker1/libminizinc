import json
import os
import subprocess
import sys
from pathlib import Path

import pytest
from minizinc import Driver, default_driver

HERE = Path(__file__).resolve().parent
BUNDLE_SCRIPT = HERE.parents[3] / "scripts" / "mzn_bundle.py"

HELPER = """% pad
% pad
function var int: boom(var int: x) =
  let { var int: y; constraint y = x div 0; } in y;
"""

MODEL = """include "helper.mzn";
var 1..3: z;
constraint boom(z) = 1;
solve satisfy;
"""

# The line of the `let` inside helper.mzn
HELPER_LINE = 4


def line_of_let(path):
    """The line the `let` of helper.mzn ends up on inside a bundle."""
    lines = path.read_text().splitlines()
    return next(i for i, l in enumerate(lines, start=1) if l.startswith("  let {"))


@pytest.fixture
def lib(tmp_path):
    """A tiny library directory, a model using it, and a dummy solver config."""
    libdir = tmp_path / "mylib"
    libdir.mkdir()
    (libdir / "helper.mzn").write_text(HELPER)
    (libdir / "extra.mzn").write_text("% nothing to see here\n")
    (tmp_path / "model.mzn").write_text(MODEL)

    solvers = tmp_path / "solvers"
    solvers.mkdir()
    # Never run: every test here compiles only. Something that exists on every
    # platform, so the configuration resolves.
    (solvers / "dummy.msc").write_text(
        json.dumps(
            {
                "id": "test.bundle.dummy",
                "name": "Dummy",
                "version": "1.0",
                "executable": sys.executable,
            }
        )
    )
    return tmp_path


def bundle(*libdirs, override=False, compress=None, output=None):
    args = [sys.executable, str(BUNDLE_SCRIPT)]
    if override:
        args.append("--override")
    if compress is not None:
        args.append("--compress" if compress else "--no-compress")
    if output is not None:
        args += ["-o", str(output)]
    args += [str(d) for d in libdirs]
    subprocess.run(args, check=True)
    return output if output is not None else libdirs[0].with_suffix(".lib.mzn")


def flatten(root, *extra_args):
    assert isinstance(default_driver, Driver)
    env = dict(os.environ, MZN_SOLVER_PATH=str(root / "solvers"))
    p = subprocess.run(
        [
            default_driver._executable,
            "--solver",
            "test.bundle.dummy",
            "-c",
            "--no-output-ozn",
            str(root / "model.mzn"),
            *extra_args,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
    )
    return p.stdout.decode()


def located_at(output, path, line):
    """Whether `output` reports a location in `path` at `line`.

    MiniZinc prints native separators, which need not match what pathlib built.
    """
    return "{}:{}".format(path, line).replace("\\", "/") in output.replace("\\", "/")


def test_replace_file_reports_original_path(lib):
    libdir = lib / "mylib"
    out = bundle(libdir, output=lib / "mylib.lib.mzn")
    assert located_at(flatten(lib, "-I", str(out)), libdir / "helper.mzn", HELPER_LINE)


def test_override_file_reports_bundle_path_and_line(lib):
    libdir = lib / "mylib"
    out = bundle(libdir, override=True, output=lib / "mylib.lib.mzn")
    assert out.read_text().startswith("/***")
    assert located_at(flatten(lib, "-I", str(out)), out, line_of_let(out))


def test_bundle_matches_directory(lib):
    libdir = lib / "mylib"
    out = bundle(libdir, output=lib / "mylib.lib.mzn")
    assert flatten(lib, "-I", str(out)) == flatten(lib, "-I", str(libdir))


def test_mznlib_list_layers_libraries(lib):
    """The first mznlib entry that has a file wins, as for a list of -G flags."""
    other = lib / "other"
    other.mkdir()
    (other / "helper.mzn").write_text(HELPER.replace("div 0", "div 1"))
    out = bundle(lib / "mylib", output=lib / "mylib.lib.mzn")

    msc = lib / "solvers" / "dummy.msc"
    config = json.loads(msc.read_text())
    config["mznlib"] = [str(other), str(out)]
    msc.write_text(json.dumps(config))
    assert "model inconsistency" not in flatten(lib)

    config["mznlib"] = [str(out), str(other)]
    msc.write_text(json.dumps(config))
    assert "model inconsistency" in flatten(lib)


def test_cmdline_globals_dir_overrides_mznlib(lib):
    other = lib / "other"
    other.mkdir()
    (other / "helper.mzn").write_text(HELPER.replace("div 0", "div 1"))

    msc = lib / "solvers" / "dummy.msc"
    config = json.loads(msc.read_text())
    config["mznlib"] = [str(lib / "mylib")]
    msc.write_text(json.dumps(config))
    assert "model inconsistency" in flatten(lib)
    assert "model inconsistency" not in flatten(lib, "-G", str(other))


def test_compression_defaults(lib):
    """The standard library is compressed; a solver library is not."""
    libdir = lib / "mylib"
    replaced = bundle(libdir, output=lib / "replace.lib.mzn")
    overridden = bundle(libdir, override=True, output=lib / "override.lib.mzn")
    plain = bundle(libdir, compress=False, output=lib / "plain.lib.mzn")
    assert replaced.read_bytes().startswith(b"@")
    assert not overridden.read_bytes().startswith(b"@")
    assert not plain.read_bytes().startswith(b"@")


def test_layers_flatten_like_the_include_path(lib):
    """Several directories bundle into one library, the first one winning."""
    base = lib / "base"
    (base / "sub").mkdir(parents=True)
    (base / "helper.mzn").write_text(HELPER.replace("div 0", "div 1"))
    (base / "sub" / "extra.mzn").write_text("% only in the lower layer\n")
    (lib / "mylib" / "sub").mkdir()
    (lib / "mylib" / "sub" / "extra.mzn").write_text("% shadowed\n")

    out = bundle(lib / "mylib", base, output=lib / "layered.lib.mzn")
    assert flatten(lib, "-I", str(out)) == flatten(
        lib, "-I", str(lib / "mylib"), "-I", str(base)
    )
    # mylib is the top layer, so its helper.mzn (the one that divides by zero) wins
    assert "model inconsistency" in flatten(lib, "-I", str(out))
