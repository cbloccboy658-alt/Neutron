# Building the stages

All 27 stages are built by a single harness:

```sh
./scripts/build_all.sh          # every stage
./scripts/build_all.sh 05 16 25 # selected stages
```

Each stage is copied to `build-out/src/stageNN` (the stage directories contain
spaces and emoji, which several of the vendored build systems mishandle), built
in `build-out/work/stageNN` and installed into `build-out/stageNN`. Per-stage
logs land in `build-out/logs/stageNN.log` and a summary is written to
`build-out/status.tsv`. A failing stage does not stop the run; the script exits
nonzero if any requested stage failed.

Overridable: `BUILD_ROOT`, `LOG_DIR`, `STATUS_FILE`, `JOBS`, `PYTHON`,
`PYTHON312`.

## Toolchain requirements

| Requirement | Needed by |
| --- | --- |
| cmake, ninja, pkg-config, autoconf/automake/libtool | most C/C++ stages |
| clang 18+ | 09 (KFR rejects GCC's handling of its `alignas` expressions, and clang 14 cannot parse its `data.hpp`) |
| gcc/g++ 13 | 14 (SSRC's CLI uses C++20 `<format>`) |
| Python 3.12+ | 07, 11, 24 (numpy/scipy/sgmse require >= 3.11) |
| Rust 1.85+ | 13 (dependencies use edition 2024) |
| perl | 27 |
| libsndfile, libmpg123, libopusfile, libogg, libvorbis, libopus, libsamplerate, libfftw3, eigen3, libyaml, libchromaprint, ffmpeg dev libs | 15, 19, 20 |

## Notes per stage

- **01 / 03 (libsndfile)** — the vendored trees are missing the public
  `include/sndfile.h`, which upstream generates and `.gitignore`s. It is
  committed here (force-added) because the CMake build lists it as a source.
- **07 (numpy) / 11 (scipy)** — the vendored trees list git submodules in
  `.gitmodules` but do not contain their contents, so the harness clones the
  ones the meson builds need. numpy is built with its own meson fork
  (`vendored-meson/meson`), which provides the `features` module its build
  requires.
- **13 (DeepFilterNet)** — built without a lockfile-pinned tract: `libDF` is
  written against ndarray 0.15 and `Graph::symbol_table`, so tract is pinned to
  0.21.5 (0.21.6 renames the field, 0.21.7 moves to ndarray 0.16).
- **19 (DynamicAudioNormalizer)** — the CLI only had Win32/MacOS definitions of
  `PUTS`, `OPEN` and `CLOSE`, and its bundled `memmem` fallback clashes with
  glibc's; both are fixed in-tree. Built serially because the makefile has no
  dependency between the API library and the CLI that links against it.
- **21 / 22 / 24** — Python projects with no compiled parts: a wheel is built
  (`build-out/stageNN/wheel`), model weights and torch are not installed.
- **23 (Apollo)** — ships no package metadata, so it is only byte-compiled.
