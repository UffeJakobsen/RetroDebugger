#!/bin/bash
#
# Subprocess-per-test CTestSuite runner for RetroDebugger.
#
# Runs every registered CTestSuite test in its OWN fresh process. This gives
# true per-test isolation (no shared global emulator/plugin state leaks between
# tests) and survives crashes/hangs: a test that segfaults or wedges fails only
# itself instead of aborting the whole run. Compare with the in-process
# `--run-suite` (continue-on-failure) mode, which is faster but shares state and
# dies if any test crashes the process.
#
# Usage:
#   tests/run_suite_isolated.sh [OPTIONS] [NAME_REGEX]
#
# Options:
#   --skip-build      Skip the xcodebuild step
#   --clean-build     Force a clean rebuild
#   --all             Include optional (removable) plugin tests; default is just
#                     core + GoatTracker
#   --timeout N       Per-test timeout in seconds (default: 90)
#   --log-dir DIR     Per-test log output dir (default: /tmp)
#   NAME_REGEX        Optional egrep pattern; only matching test names run
#
# Output:
#   tests/results/isolated_run.txt   per-test verdict + summary
#   <log-dir>/isolated-<test>.log    captured stdout/stderr for non-PASS tests
#
# Exit code: 0 if every run test PASSed, 1 otherwise.

set -uo pipefail   # deliberately NOT -e: we must continue past failing children

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

XCODE_PROJECT="$PROJECT_DIR/platform/MacOS/c64d.xcodeproj"
BUILD_DIR="$PROJECT_DIR/platform/MacOS/DerivedData"
RESULTS_DIR="$PROJECT_DIR/tests/results"
RESULTS_FILE="$RESULTS_DIR/isolated_run.txt"
LIST_FILE="$RESULTS_DIR/test_list.txt"
PERTEST_RESULT="$RESULTS_DIR/last_run.txt"

SKIP_BUILD=false
CLEAN_BUILD=false
TIMEOUT=90
LOG_DIR="/tmp"
NAME_REGEX=""
ALL_PLUGINS=""   # set to --all-plugin-tests to include optional (removable) plugins

while [ $# -gt 0 ]; do
    case "$1" in
        --skip-build)  SKIP_BUILD=true ;;
        --clean-build) CLEAN_BUILD=true ;;
        --all)         ALL_PLUGINS="--all-plugin-tests" ;;
        --timeout)     TIMEOUT="$2"; shift ;;
        --log-dir)     LOG_DIR="$2"; shift ;;
        --*)           echo "ERROR: unknown option $1"; exit 2 ;;
        *)             NAME_REGEX="$1" ;;
    esac
    shift
done

APP_BINARY=""
APP_BINARY_MTIME=0
select_newest_binary() {
    local candidate
    local mtime
    for candidate in "$@"; do
        if [ -f "$candidate" ]; then
            mtime=$(stat -f "%m" "$candidate" 2>/dev/null || printf '0')
            if [ "$mtime" -gt "$APP_BINARY_MTIME" ]; then
                APP_BINARY="$candidate"
                APP_BINARY_MTIME="$mtime"
            fi
        fi
    done
}

mkdir -p "$RESULTS_DIR"

# --- Build -----------------------------------------------------------------
if [ "$SKIP_BUILD" = false ]; then
    echo "=== Building Retro Debugger ==="
    BUILD_ARGS=(-project "$XCODE_PROJECT" -scheme "Retro Debugger" -derivedDataPath "$BUILD_DIR")
    if [ "$CLEAN_BUILD" = true ]; then
        BUILD_ARGS+=(clean build)
    fi
    if ! xcodebuild "${BUILD_ARGS[@]}" -quiet 2>&1; then
        echo "BUILD FAILED"
        exit 2
    fi
    echo "=== Build succeeded ==="
fi

# --- Locate binary ---------------------------------------------------------
# Pick the GLOBALLY newest binary across both DerivedData locations in a single
# pass. (Tiered "BUILD_DIR first, else ~/Library" selection is wrong: a stale
# binary under BUILD_DIR would shadow a freshly built one in ~/Library and, if
# it predates a new flag like --list-tests, the runner would hang forever.)
select_newest_binary \
    "$BUILD_DIR"/Build/Products/Release/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$BUILD_DIR"/Build/Products/Debug/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$BUILD_DIR"/*/Build/Products/Release/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$BUILD_DIR"/*/Build/Products/Debug/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$HOME"/Library/Developer/Xcode/DerivedData/*/Build/Products/Release/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$HOME"/Library/Developer/Xcode/DerivedData/*/Build/Products/Debug/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger"
if [ -z "$APP_BINARY" ]; then
    echo "ERROR: Could not find Retro Debugger binary. Build first."
    exit 2
fi
echo "Binary: $APP_BINARY"

# --- Enumerate tests -------------------------------------------------------
# Bounded: a binary without --list-tests would otherwise launch normally and
# never exit, hanging the runner.
rm -f "$LIST_FILE"
"$APP_BINARY" --headless --log-dir "$LOG_DIR" --list-tests $ALL_PLUGINS >/dev/null 2>&1 &
list_pid=$!
list_waited=0
while kill -0 "$list_pid" 2>/dev/null; do
    if [ "$list_waited" -ge 30 ]; then
        kill -9 "$list_pid" 2>/dev/null
        wait "$list_pid" 2>/dev/null
        echo "ERROR: --list-tests did not exit within 30s (stale binary without the flag?)"
        exit 2
    fi
    sleep 1
    list_waited=$((list_waited + 1))
done
wait "$list_pid" 2>/dev/null || true
if [ ! -s "$LIST_FILE" ]; then
    echo "ERROR: --list-tests produced no test list at $LIST_FILE"
    exit 2
fi

# test_list.txt lines are "<category>\t<name>" (category = "core" or plugin name).
# Older binaries wrote bare "<name>"; tolerate both. Parallel indexed arrays
# (not an associative array) so this works on macOS's stock bash 3.2.
TESTS=()
CATS=()
while IFS=$'\t' read -r f1 f2; do
    if [ -n "$f2" ]; then category="$f1"; name="$f2"; else category="core"; name="$f1"; fi
    [ -z "$name" ] && continue
    if [ -n "$NAME_REGEX" ] && ! echo "$name" | grep -Eq "$NAME_REGEX"; then
        continue
    fi
    TESTS+=("$name")
    CATS+=("$category")
done < "$LIST_FILE"

TOTAL=${#TESTS[@]}
if [ "$TOTAL" -eq 0 ]; then
    echo "ERROR: no tests matched ${NAME_REGEX:-<all>}"
    exit 2
fi
echo "Running $TOTAL test(s) in isolated processes (timeout ${TIMEOUT}s each)..."

# --- Per-test runner with portable timeout ---------------------------------
# Returns: 0 child-exit-0, child exit code otherwise, 124 on timeout.
LAST_EXIT=0
run_one() {
    local name="$1"
    local out="$2"
    "$APP_BINARY" --headless --log-dir "$LOG_DIR" --run-test "$name" --exit-after-tests \
        >"$out" 2>&1 &
    local pid=$!
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$waited" -ge "$TIMEOUT" ]; then
            kill -9 "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            LAST_EXIT=124
            return
        fi
        sleep 1
        waited=$((waited + 1))
    done
    wait "$pid"
    LAST_EXIT=$?
}

# --- Run -------------------------------------------------------------------
: > "$RESULTS_FILE"
PASS=0; FAIL=0; CRASH=0; TIMEOUT_CNT=0
idx=0
while [ "$idx" -lt "$TOTAL" ]; do
    name="${TESTS[$idx]}"
    cat="${CATS[$idx]}"
    idx=$((idx + 1))
    rm -f "$PERTEST_RESULT"
    out="$LOG_DIR/isolated-$name.log"
    run_one "$name" "$out"
    code=$LAST_EXIT

    # Classify. Prefer the binary's own RESULT line; fall back to exit code.
    result_line=""
    [ -f "$PERTEST_RESULT" ] && result_line=$(grep "^RESULT:" "$PERTEST_RESULT" 2>/dev/null || true)

    if [ "$code" -eq 124 ]; then
        verdict="TIMEOUT"; TIMEOUT_CNT=$((TIMEOUT_CNT + 1))
    elif [ "$code" -ge 128 ]; then
        verdict="CRASH(sig$((code - 128)))"; CRASH=$((CRASH + 1))
    elif echo "$result_line" | grep -q "^RESULT: 1/1"; then
        verdict="PASS"; PASS=$((PASS + 1))
    elif echo "$result_line" | grep -q "^RESULT: 0/1"; then
        verdict="FAIL"; FAIL=$((FAIL + 1))
    elif [ "$code" -eq 0 ]; then
        # exited cleanly but no parseable per-test result (e.g. skipped/no result)
        verdict="PASS"; PASS=$((PASS + 1))
    else
        verdict="CRASH(exit$code)"; CRASH=$((CRASH + 1))
    fi

    # Pull the one-line summary the test wrote, if any.
    summary=""
    [ -f "$PERTEST_RESULT" ] && summary=$(grep -E "^\[$name\] " "$PERTEST_RESULT" 2>/dev/null | head -1 || true)

    printf '%-18s %-40s %-14s %s\n' "$cat" "$name" "$verdict" "$summary" | tee -a "$RESULTS_FILE"

    # Keep logs only for non-PASS, to aid triage; drop noise otherwise.
    if [ "$verdict" = "PASS" ]; then
        rm -f "$out"
    fi
done

# --- Summary ---------------------------------------------------------------
{
    echo "---"
    echo "RESULT: $PASS/$TOTAL passed  (fail=$FAIL crash=$CRASH timeout=$TIMEOUT_CNT)"
} | tee -a "$RESULTS_FILE"

echo ""
echo "Full report: $RESULTS_FILE"
if [ "$PASS" -eq "$TOTAL" ]; then
    echo "ALL TESTS PASSED ($PASS/$TOTAL)"
    exit 0
else
    echo "TESTS FAILED ($PASS/$TOTAL passed; fail=$FAIL crash=$CRASH timeout=$TIMEOUT_CNT)"
    echo "Non-PASS logs kept under $LOG_DIR/isolated-*.log"
    exit 1
fi
