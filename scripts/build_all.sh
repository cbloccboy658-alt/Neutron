#!/usr/bin/env bash
# Build every stage of the Neutron audio pipeline from source.
#
#   ./scripts/build_all.sh              # build all 27 stages
#   ./scripts/build_all.sh 05 16 25     # build only the given stages
#
# Artifacts are installed under $BUILD_ROOT (default: build-out/) and per-stage
# logs are written to $LOG_DIR (default: build-out/logs/).  Nothing is written
# into the vendored source trees except each project's own build directory.

set -u -o pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$REPO_ROOT/build-out}"
LOG_DIR="${LOG_DIR:-$BUILD_ROOT/logs}"
JOBS="${JOBS:-$(nproc)}"
PYTHON="${PYTHON:-python3}"

mkdir -p "$BUILD_ROOT" "$LOG_DIR"

STATUS_FILE="${STATUS_FILE:-$BUILD_ROOT/status.tsv}"
: > "$STATUS_FILE"

log()  { printf '\n=== %s\n' "$*"; }

stage_dir() { # stage_dir 05 -> path of the vendored source tree for stage 05
  local n="$1"
  local top
  top="$(find "$REPO_ROOT" -maxdepth 1 -type d -name "[[]Stage $n]*" -print -quit)"
  [ -n "$top" ] || return 1
  # descend through wrapper directories until the vendored source tree is found
  local cur="$top" child
  while ! has_build_file "$cur"; do
    [ "$(find "$cur" -mindepth 1 -maxdepth 1 | wc -l)" -eq 1 ] || break
    child="$(find "$cur" -mindepth 1 -maxdepth 1)"
    [ -d "$child" ] || break
    cur="$child"
  done
  printf '%s' "$cur"
}

has_build_file() {
  local d="$1" f
  for f in CMakeLists.txt configure configure.ac Makefile Makefile.PL setup.py pyproject.toml Cargo.toml waf; do
    [ -e "$d/$f" ] && return 0
  done
  return 1
}

prefix_for() { printf '%s/stage%s' "$BUILD_ROOT" "$1"; }

# The vendored stage directories contain spaces, brackets and emoji, which break
# CMake, autotools and several other build systems.  Each source tree is copied
# once into a plain path under $BUILD_ROOT/src and built from there.
staged_copy() { # staged_copy <stage> <src> -> echoes the space-free source path
  local stage="$1" src="$2" dst="$BUILD_ROOT/src/stage$1"
  if [ ! -d "$dst" ]; then
    mkdir -p "$BUILD_ROOT/src" || return 1
    cp -a "$src" "$dst.tmp" >&2 || return 1
    mv "$dst.tmp" "$dst" || return 1
  fi
  printf '%s' "$dst"
}

# --- build helpers -----------------------------------------------------------

build_cmake() { # build_cmake <stage> <src> [extra cmake args...]
  local stage="$1" src="$2"; shift 2
  local bdir="$BUILD_ROOT/work/stage$stage" prefix; prefix="$(prefix_for "$stage")"
  cmake -S "$src" -B "$bdir" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" -DBUILD_TESTING=OFF "$@" &&
  cmake --build "$bdir" -j "$JOBS" &&
  cmake --install "$bdir"
}

build_autotools() { # build_autotools <stage> <src> [configure args...]
  local stage="$1" src="$2"; shift 2
  local prefix; prefix="$(prefix_for "$stage")"
  if [ ! -x "$src/configure" ]; then
    ( cd "$src" && { [ -x ./autogen.sh ] && NOCONFIGURE=1 ./autogen.sh || autoreconf -fi; } ) || return 1
  fi
  # some autogen.sh scripts configure in-tree, so build in-tree as well
  ( cd "$src" && [ ! -f Makefile ] || make distclean >/dev/null 2>&1;
    cd "$src" && ./configure --prefix="$prefix" "$@" && make -j "$JOBS" && make install )
}

venv_for() { # venv_for <stage> [interpreter] -> creates (once) and echoes the venv python
  local stage="$1" py="${2:-$PYTHON}" venv
  venv="$BUILD_ROOT/venv$stage-$(basename "$py")"
  [ -x "$venv/bin/python" ] || "$py" -m venv "$venv" >&2 || return 1
  "$venv/bin/pip" install -q --upgrade pip wheel >&2 || return 1
  printf '%s' "$venv/bin/python"
}

build_python_wheel() { # build_python_wheel <stage> <src> [interpreter]  (wheel only, no deps)
  local stage="$1" src="$2" py
  py="$(venv_for "$stage" "${3:-$PYTHON}")" || return 1
  local out; out="$(prefix_for "$stage")/wheel"
  mkdir -p "$out"
  "$py" -m pip wheel --no-deps -w "$out" "$src"
}

build_python_install() { # build_python_install <stage> <src> [interpreter]
  local stage="$1" src="$2" py
  py="$(venv_for "$stage" "${3:-$PYTHON}")" || return 1
  "$py" -m pip install -v "$src"
}

# The vendored numpy/scipy trees list git submodules in .gitmodules but do not
# contain their contents, and their meson builds refuse to run without them.
fetch_submodules() { # fetch_submodules <src> <path>...
  local src="$1"; shift
  local path url
  for path in "$@"; do
    [ -n "$(ls -A "$src/$path" 2>/dev/null)" ] && continue
    url="$(awk -v p="$path" '$1=="path"&&$3==p{f=1;next} f&&$1=="url"{print $3;exit}' "$src/.gitmodules")"
    [ -n "$url" ] || { echo "no submodule url for $path" >&2; return 1; }
    rm -rf "$src/$path" &&
    git clone --depth 1 --recurse-submodules --shallow-submodules "$url" "$src/$path" || return 1
  done
}

# numpy and scipy point meson-python at a vendored meson checkout that is not
# part of this repository; MESON redirects it to the one installed in the venv.
build_python_meson() { # build_python_meson <stage> <src> [meson]
  local stage="$1" src="$2" meson="${3:-}" py venv
  py="$(venv_for "$stage" "$PYTHON312")" || return 1
  venv="$(dirname "$py")"
  "$py" -m pip install -q meson ninja || return 1
  MESON="${meson:-$venv/meson}" "$py" -m pip install -v "$src"
}

find_python312() {
  local p
  for p in python3.14 python3.13 python3.12 "$PYTHON"; do
    command -v "$p" >/dev/null && { printf '%s' "$p"; return 0; }
  done
  printf '%s' "$PYTHON"
}
PYTHON312="${PYTHON312:-$(find_python312)}"

python_syntax_check() { # python_syntax_check <stage> <src> [subdir]
  local stage="$1" src="$2" sub="${3:-.}"
  "$PYTHON" -m compileall -q -j "$JOBS" "$src/$sub"
}

# --- per-stage recipes -------------------------------------------------------

stage_01() { build_cmake 01 "$1" -DBUILD_PROGRAMS=ON -DBUILD_EXAMPLES=OFF -DENABLE_EXTERNAL_LIBS=ON; }
stage_02() { build_cmake 02 "$1" -DWAVPACK_BUILD_PROGRAMS=ON -DWAVPACK_BUILD_DOCS=OFF; }
stage_03() { build_cmake 03 "$1" -DBUILD_PROGRAMS=ON -DBUILD_EXAMPLES=OFF -DENABLE_EXTERNAL_LIBS=ON; }
stage_04() { build_cmake 04 "$1" -DBUILD_TESTS=OFF -DWITH_OPENMP=OFF; }
stage_05() { build_ffmpeg 05 "$1"; }
stage_06() { build_cmake 06 "$1" -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DWITH_FORTIFY_SOURCE=OFF -DINSTALL_MANPAGES=OFF; }
stage_07() { # numpy builds with its own meson fork, which provides the "features" module
  local src="$1"
  fetch_submodules "$src" vendored-meson/meson numpy/_core/src/umath/svml \
    numpy/_core/src/npysort/x86-simd-sort numpy/_core/src/highway \
    numpy/fft/pocketfft numpy/_core/src/common/pythoncapi-compat &&
  build_python_meson 07 "$src" "$src/vendored-meson/meson/meson.py"
}
stage_08() { build_cmake 08 "$1" -DBUILD_TESTS=OFF -DWITH_OPENMP=OFF; }
# KFR requires a recent Clang: GCC rejects its non-constant alignas expressions
# and Clang 14 miscompiles kfr/audio/data.hpp.
stage_09() {
  local c
  for c in 20 19 18; do
    command -v "clang-$c" >/dev/null && CLANG_CC="clang-$c" && CLANG_CXX="clang++-$c" && break
  done
  # clang picks the newest libstdc++ on the box; pin it to a version it can parse
  local gxx_inc=""
  for v in 11 12; do
    [ -d "/usr/include/c++/$v" ] &&
      gxx_inc="-nostdinc++ -isystem /usr/include/c++/$v -isystem /usr/include/x86_64-linux-gnu/c++/$v" && break
  done
  CC="${CLANG_CC:-clang}" CXX="${CLANG_CXX:-clang++}" build_cmake 09 "$1" -DENABLE_TESTS=OFF \
    -DKFR_ENABLE_DFT=ON -DCMAKE_CXX_FLAGS="$gxx_inc"
}
stage_10() { build_cmake 10 "$1"; }
stage_11() {
  local src="$1"
  fetch_submodules "$src" subprojects/array_api_compat subprojects/array_api_extra \
    subprojects/boost_math/math subprojects/cobyqa subprojects/highs \
    subprojects/unuran subprojects/xsf &&
  build_python_meson 11 "$src"
}
stage_12() { build_cmake 12 "$1"; }
stage_13() { # DeepFilterNet: Rust workspace (native libDF + CLI)
  local src="$1"
  # libDF is written against ndarray 0.15 (tract moved to 0.16 in 0.21.7) and
  # against Graph::symbol_table, which tract renamed in 0.21.6
  ( cd "$src" && cargo update --precise 0.21.5 \
      -p tract-core -p tract-onnx -p tract-pulse -p tract-hir ) &&
  ( cd "$src" && CARGO_TARGET_DIR="$BUILD_ROOT/work/stage13" \
      cargo build --release -p deep_filter --lib --bin deep-filter \
        --features bin,tract,wav-utils,transforms ) &&
  mkdir -p "$(prefix_for 13)/bin" "$(prefix_for 13)/lib" &&
  cp "$BUILD_ROOT/work/stage13/release/deep-filter" "$(prefix_for 13)/bin/" &&
  cp "$BUILD_ROOT"/work/stage13/release/libdf.* "$(prefix_for 13)/lib/"
}
# SSRC's CLI uses C++20 <format>, which needs GCC 13 or newer.
stage_14() { CC="${GCC13_CC:-gcc-13}" CXX="${GCC13_CXX:-g++-13}" build_cmake 14 "$1" -DBUILD_TESTS=OFF; }
stage_15() { # essentia: waf build system
  local src="$1" prefix; prefix="$(prefix_for 15)"
  ( cd "$src" && "$PYTHON" waf configure --prefix="$prefix" --build-static --lightweight= --fft=KISS &&
    "$PYTHON" waf -j "$JOBS" && "$PYTHON" waf install )
}
stage_16() { build_ffmpeg 16 "$1"; }
stage_17() { build_cmake 17 "$1"; }
stage_18() { build_cmake 18 "$1"; }
stage_19() { # DynamicAudioNormalizer: plain Makefile
  local src="$1" prefix; prefix="$(prefix_for 19)"
  # the top-level makefile has no dependency between the API and the CLI, so the
  # CLI can start linking before the API library exists: build serially
  ( cd "$src" && make -j1 MODE=minimal DynamicAudioNormalizerAPI DynamicAudioNormalizerCLI ) &&
  mkdir -p "$prefix" &&
  find "$src" -maxdepth 3 -type f \( -name 'DynamicAudioNormalizerCLI*.bin' -o -name 'libDynamicAudioNormalizerAPI*' \) \
    -exec cp {} "$prefix/" \;
}
stage_20() { build_autotools 20 "$1"; }
stage_21() { build_python_wheel 21 "$1"; }
stage_22() { build_python_wheel 22 "$1"; }
stage_23() { python_syntax_check 23 "$1"; }  # Apollo ships no package metadata
stage_24() { build_python_wheel 24 "$1" "$PYTHON312"; }  # sgmse requires Python >= 3.11
stage_25() { build_ffmpeg 25 "$1"; }
stage_26() { build_python_install 26 "$1"; }
stage_27() { # exiftool: perl ExtUtils::MakeMaker
  local src="$1" prefix; prefix="$(prefix_for 27)"
  ( cd "$src" && perl Makefile.PL PREFIX="$prefix" && make -j "$JOBS" && make install )
}

build_ffmpeg() { # build_ffmpeg <stage> <src>
  local stage="$1" src="$2"
  local bdir="$BUILD_ROOT/work/stage$stage" prefix; prefix="$(prefix_for "$stage")"
  mkdir -p "$bdir" || return 1
  ( cd "$bdir" && "$src/configure" --prefix="$prefix" --disable-doc --disable-debug \
      --enable-gpl --enable-libvorbis --enable-libopus &&
    make -j "$JOBS" && make install )
}

# --- driver ------------------------------------------------------------------

ALL_STAGES=(01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27)
STAGES=("$@")
[ "${#STAGES[@]}" -gt 0 ] || STAGES=("${ALL_STAGES[@]}")

rc_all=0
for s in "${STAGES[@]}"; do
  src="$(stage_dir "$s")"
  if [ -z "$src" ]; then
    printf '%s\tMISSING\t-\n' "$s" >> "$STATUS_FILE"; rc_all=1; continue
  fi
  log "Stage $s: $src"
  start=$SECONDS
  if ! src="$(staged_copy "$s" "$src")"; then
    printf '%s\tCOPY-FAILED\t-\n' "$s" >> "$STATUS_FILE"; rc_all=1; continue
  fi
  if "stage_$s" "$src" > "$LOG_DIR/stage$s.log" 2>&1; then
    printf '%s\tOK\t%ss\n' "$s" "$((SECONDS - start))" >> "$STATUS_FILE"
    echo "Stage $s OK ($((SECONDS - start))s)"
  else
    printf '%s\tFAILED\t%ss\n' "$s" "$((SECONDS - start))" >> "$STATUS_FILE"
    echo "Stage $s FAILED ($((SECONDS - start))s) - see $LOG_DIR/stage$s.log"
    tail -20 "$LOG_DIR/stage$s.log"
    rc_all=1
  fi
done

log "Summary"
awk -F'\t' '{printf "  stage %-4s %-12s %s\n", $1, $2, $3}' "$STATUS_FILE"
exit "$rc_all"
