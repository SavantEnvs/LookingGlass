#!/usr/bin/env bash
#
# mayhem/test.sh: RUNS the known-answer selftest that mayhem/build.sh built with normal flags.
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
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

# Behavioral oracle: mayhem/selftest.c exercises the option parser, the damage-rect math and the
# framebuffer copy code the fuzz targets cover, and prints exact results. Every expected line below is
# a known input -> known output; a missing binary, a no-op build or a broken fix fails them all.
SELFTEST="$SRC/mayhem/build/selftest"
if [ ! -x "$SELFTEST" ]; then
  echo "missing $SELFTEST: mayhem/build.sh did not produce it" >&2
  emit_ctrf "lookingglass-selftest" 0 1 0
  exit $?
fi

OUT=$(TMPDIR="${TMPDIR:-/tmp}" "$SELFTEST" 2>/dev/null)
rc=$?

EXPECTED=(
  'ini load=1'
  'ini validate=1'
  'ini width=800 title=[hello world] vsync=1 scale=2.50 port=1234 host=localhost'
  '[net]'
  'port=1234'
  'host=localhost'
  'args width=1920 title=[My Title] vsync=1 scale=0.50 port=77'
  'rects merged=2 first=0,0,15,15 second=40,40,4,4'
  'rects kept=2'
  'rects tofb=1'
  'rects fb row1=0,11,12,13,0,0 row2=19,21,0 sum=96'
  'rects oob=0'
  'rects fromfb=1'
  'rects dst row1=0,11,13,0 row2=19,21,0,0'
  'fb write=1'
  'fb wait=1 toobig=0'
  'fb read=1'
  'fb dst=0,21,36,57,72,93'
  'fb bad=0'
)

passed=0; failed=0
[ "$rc" -eq 0 ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL selftest exit status $rc" >&2; }
for line in "${EXPECTED[@]}"; do
  if printf '%s\n' "$OUT" | grep -qxF -- "$line"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1)); echo "FAIL missing line: $line" >&2
  fi
done
# and nothing else: the non-blank output line count must match
[ "$(printf '%s\n' "$OUT" | grep -c .)" -eq "${#EXPECTED[@]}" ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL unexpected number of output lines" >&2; }

emit_ctrf "lookingglass-selftest" "$passed" "$failed" 0
