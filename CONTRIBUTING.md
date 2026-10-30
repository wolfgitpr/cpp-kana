# Contributing

## Build and test

The library needs CMake 3.17 or newer and a C++17 compiler. Using the presets in `CMakePresets.json` needs
CMake 3.21 or newer.

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

`default` is Release, shared library, tests and install enabled; `static` builds the archive form; `sanitize`
builds with AddressSanitizer and UndefinedBehaviorSanitizer and refuses to configure when the toolchain
cannot provide them. Build trees land under `.tmp/`.

`CPP_KANA2_BUILD_TESTS` is the test switch and the only one consulted; with it off, configure says out loud
that no tests are configured instead of staying silent. It requires a Python 3 interpreter, because several
checks are Python scripts.

Run the whole suite the way CI does — configure, build, ctest through a preset, then the gates explicitly:

```bash
python tools/run_all_gates.py
```

Everything this machine can verify, in one command, including the legs (Clang, static, the two bindings) and an
explicit list of the legs it cannot run and why:

```bash
python tools/verify_all.py
```

## Gates

`tools/run_all_gates.py` holds the gate registry. Every gate is also a ctest test named `Gate_<gate>`; without
a Python 3 interpreter the gates are not registered and the C++ suite still runs.

Each gate has a `--self-test` that drives a synthetic failure through it, and the runner executes that
self-test before the real check. **A gate's verdict is only trustworthy once its self-test passes**: if a
self-test fails, the run stops rather than reporting the checks that follow. When adding a gate, wire it into
the registry, into `CPP_KANA2_GATES` in `tests/CMakeLists.txt`, and give it a `--self-test`;
`tools/check_gate_inventory.py` fails when those three lists disagree.

Conformance changes are re-pinned, not overwritten. After an intended engine change:

```bash
.tmp/build-default/bin/cpp-kana2-conformance \
    --corpus tests/data/corpus/cases.tsv --dir . --write    # regenerate the fixture
python tools/check_conformance.py --write                   # re-pin conformance/cases.sha256
```

The fixture is compared byte for byte by the C++, Python and C# suites, so an engine change that nobody
re-pinned fails `ctest` instead of quietly rewriting the lock.

Bindings and gates:

```bash
python python/tools/build_native.py      # stage the native library + _version.py
python -m pytest python/tests -q         # Python suite
python csharp/tools/run_tests.py         # C# suite (dotnet; reuses the staged library by default)
python tools/check_bindings.py           # the bindings must transcribe the C ABI exactly
```

## Layering

`src/` is layered, and the layer table in [tools/check_layers.py](tools/check_layers.py) is the authority:

```
infra < data < mora < core < c_api
```

A layer may include only the layers to its left; a file under `src/` that belongs to no layer is an error, not
something to skip. Public headers under `include/cpp-kana2/` are the installed surface: they may be included
from anywhere, but a public header must not escape into `src/` with a `../` include, because it is installed
on its own. `tools/check_layers.py` enforces both rules and its self-test proves it can report an upward
edge, an unclassified file and a public-header escape.

## The frozen surface

`tests/abi/` holds three checked-in lists and they are the authority:

- `expected_public_headers.txt` — the installed headers;
- `expected_c_api_symbols.txt` — the `cppk2_*` entry points;
- `expected_public_names.txt` — namespace-level names in `cpp_kana::v2`.

`tools/check_public_surface.py` compares them with the tree in both directions: a name added without touching
the list is red, and a name in the list that the headers no longer declare is red. Freezing is a deliberate
act and the diff is the review:

```bash
python tools/check_public_surface.py --freeze   # rewrite the lists from the current headers
python tools/check_public_surface.py            # verify
python tools/check_public_surface.py --self-test
```

Changing the C ABI means changing the C++ side, `c_api.h`, both bindings and these lists in the same commit;
`tools/check_bindings.py` fails otherwise. The C ABI's own rules — append-only status codes, `struct_size` on
structures a caller fills, no new members inside 2.x for structures the library fills — are stated at the top
of `include/cpp-kana2/ext/c_api.h`.

## Line endings

[.gitattributes](.gitattributes) is the policy:

- `conformance/cases.json` and `conformance/cases.sha256` are `text eol=lf`, because the checksum is over
  the file's bytes — a CRLF checkout would hash differently on different machines.
- `data/**` and `tests/data/**` are `-text`: git must not normalise data at all, so the bytes a reader sees
  are the bytes committed and the bytes the gates hash.
- Sources and scripts (`*.c`, `*.cpp`, `*.h`, `*.py`, `*.cs`, `*.cmake`, `CMakeLists.txt`, `*.sh`, `*.md`,
  ...) are `text eol=lf`.

There is deliberately no blanket `*` rule. If you add a file whose bytes are pinned by a gate or a checksum,
add the corresponding `.gitattributes` line in the same change.

## Style

Formatting is clang-format's, with `.clang-format` at the repository root. Source lists in CMake are explicit
(no globbing); `tools/check_source_list.py` fails when a file and its list disagree.
