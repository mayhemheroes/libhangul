#!/usr/bin/env bash
#
# mayhem/test.sh — RUN libhangul's check-based unit test suite (built by mayhem/build.sh
# into build-tests/). Asserts behavior (preedit/commit strings, jamo conversions, hanja
# lookups) via libcheck assertions; emits a CTRF summary.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

runner="$SRC/build-tests/test/unittest"
if [ ! -x "$runner" ]; then
  echo "FATAL: $runner missing — mayhem/build.sh should have built it" >&2
  emit_ctrf "check" 0 1
  exit 1
fi

out="$("$runner" 2>&1)"; rc=$?
echo "$out"
# libcheck summary line: "100%: Checks: 28, Failures: 0, Errors: 0"
checks=$(echo "$out" | sed -n 's/.*Checks: \([0-9]*\),.*/\1/p' | tail -1)
failures=$(echo "$out" | sed -n 's/.*Failures: \([0-9]*\),.*/\1/p' | tail -1)
errors=$(echo "$out" | sed -n 's/.*Errors: \([0-9]*\).*/\1/p' | tail -1)
if [ -z "$checks" ] || [ -z "$failures" ] || [ -z "$errors" ]; then
  echo "FATAL: could not parse libcheck summary (runner exit $rc)" >&2
  emit_ctrf "check" 0 1
  exit 1
fi
failed=$(( failures + errors ))
emit_ctrf "check" $(( checks - failed )) "$failed"
