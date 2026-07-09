#!/usr/bin/env bash
#
# mayhem/build.sh — build libhangul's fuzz harnesses, tools, and test suite.
#
# 1) sanitized static libhangul + tools (build-fuzz/) — the fuzzed code is instrumented
# 2) the libFuzzer harness + a standalone (run-once) reproducer per harness
# 3) the project's own check-based unit test suite with NORMAL flags (build-tests/)
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# 1) Sanitized build: static libhangul + the `hangul` CLI tool (a file-input fuzz target).
cmake -B build-fuzz \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS" \
    -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DENABLE_TOOLS=ON
cmake --build build-fuzz -j"$MAYHEM_JOBS"
install -m755 build-fuzz/tools/hangul /mayhem/hangul-tool

# 2) The libFuzzer harness, plus a standalone run-once reproducer.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
    "$SRC/mayhem/fuzz_hangul_jamo_to_cjamo.cpp" \
    -I"$SRC/hangul" build-fuzz/hangul/libhangul.a \
    -o /mayhem/fuzz_hangul_jamo_to_cjamo
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o /tmp/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS \
    "$SRC/mayhem/fuzz_hangul_jamo_to_cjamo.cpp" /tmp/standalone_main.o \
    -I"$SRC/hangul" build-fuzz/hangul/libhangul.a \
    -o /mayhem/fuzz_hangul_jamo_to_cjamo-standalone

# 3) Test suite with NORMAL flags (independent build) — test.sh only RUNS it.
cmake -B build-tests \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DENABLE_UNIT_TEST=ON
cmake --build build-tests -j"$MAYHEM_JOBS"
cmake --build build-tests -j"$MAYHEM_JOBS" --target unittest
