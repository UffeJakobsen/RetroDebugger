#!/bin/bash
#
# CLI Test Runner for RetroDebugger
#
# Usage:
#   tests/run_test.sh [OPTIONS] [TestName] [-- APP_OPTIONS...]
#
# Options:
#   --skip-build    Skip the xcodebuild step
#   --clean-build   Force a clean rebuild before running tests
#   --visible       Run without --headless
#   --imgui-tests   Run all ImGui UI tests
#   --imgui-test FILTER  Run ImGui UI tests matching FILTER
#   --timeout N     Set timeout in seconds (default: 60)
#   --log-dir DIR   Set log output directory (default: /tmp)
#   --layouts-fixture FILE  Copy layout fixture to /tmp and pass via --layouts-file
#
# App options:
#   Pass extra Retro Debugger arguments after --
#   Example: -- --layouts-file tests/data/layouts-test.dat
#
# Examples:
#   tests/run_test.sh                            # Run all suite tests
#   tests/run_test.sh EmulatorStartup            # Run single test
#   tests/run_test.sh --skip-build EmulatorStartup  # Skip build, run single test
#   tests/run_test.sh --imgui-tests              # Run all ImGui UI tests
#   tests/run_test.sh --imgui-test open_all_views  # Run filtered ImGui UI tests
#   tests/run_test.sh --visible LayoutSmoke -- --layouts-file tests/data/layouts-test.dat
#   tests/run_test.sh --layouts-fixture tests/data/layouts-test.dat AutoLayoutPreservation

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_BUILD=false
CLEAN_BUILD=false
VISIBLE=false
RUN_IMGUI_ALL=false
RUN_IMGUI_FILTER=""
TIMEOUT=60
TEST_NAME=""
LOG_DIR="/tmp"
LAYOUTS_FIXTURE=""
EXTRA_APP_ARGS=()
RESERVED_APP_FLAGS=(--run-test --run-suite --run-tests --run-imgui-test --exit-after-tests --headless --visible)
COPIED_LAYOUTS_FILE=""
COPIED_LAYOUTS_DIR=""
FORWARDED_LAYOUTS_FILE=""
FORWARDED_LAYOUTS_FILE_COUNT=0
KNOWN_LAYOUTS_FIXTURE_PATH="$PROJECT_DIR/tests/data/layouts-test.dat"
COPIED_LAYOUTS_MATCHES_KNOWN_FIXTURE=false

cleanup_temp_layouts_file() {
    if [ -n "$COPIED_LAYOUTS_FILE" ] && [ -f "$COPIED_LAYOUTS_FILE" ]; then
        rm -f "$COPIED_LAYOUTS_FILE"
    fi
    if [ -n "$COPIED_LAYOUTS_DIR" ] && [ -d "$COPIED_LAYOUTS_DIR" ]; then
        rmdir "$COPIED_LAYOUTS_DIR" 2>/dev/null || true
    fi
}

trap cleanup_temp_layouts_file EXIT

copy_layouts_file_to_tmp() {
    local source_path="$1"

    COPIED_LAYOUTS_DIR="$(mktemp -d "/tmp/retrodebugger-layouts.XXXXXX")"
    COPIED_LAYOUTS_FILE="$COPIED_LAYOUTS_DIR/layouts.dat"
    cp "$source_path" "$COPIED_LAYOUTS_FILE"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --clean-build)
            CLEAN_BUILD=true
            shift
            ;;
        --visible)
            VISIBLE=true
            shift
            ;;
        --imgui-tests)
            RUN_IMGUI_ALL=true
            shift
            ;;
        --imgui-test)
            if [ $# -lt 2 ]; then
                echo "ERROR: --imgui-test requires a filter"
                exit 2
            fi
            RUN_IMGUI_FILTER="$2"
            shift 2
            ;;
        --timeout)
            if [ $# -lt 2 ]; then
                echo "ERROR: --timeout requires a value"
                exit 2
            fi
            if ! [[ "$2" =~ ^[0-9]+$ ]]; then
                echo "ERROR: --timeout requires an integer number of seconds"
                exit 2
            fi
            TIMEOUT="$2"
            shift 2
            ;;
        --log-dir)
            if [ $# -lt 2 ]; then
                echo "ERROR: --log-dir requires a directory"
                exit 2
            fi
            LOG_DIR="$2"
            shift 2
            ;;
        --layouts-fixture)
            if [ $# -lt 2 ]; then
                echo "ERROR: --layouts-fixture requires a file"
                exit 2
            fi
            LAYOUTS_FIXTURE="$2"
            shift 2
            ;;
        --)
            shift
            EXTRA_APP_ARGS=("$@")
            break
            ;;
        *)
            TEST_NAME="$1"
            shift
            ;;
    esac
done

if [ "$CLEAN_BUILD" = true ]; then
    SKIP_BUILD=false
fi

if [ "$RUN_IMGUI_ALL" = true ] && [ -n "$RUN_IMGUI_FILTER" ]; then
    echo "ERROR: Use either --imgui-tests or --imgui-test, not both"
    exit 2
fi

RUN_IMGUI_MODE=false
if [ "$RUN_IMGUI_ALL" = true ] || [ -n "$RUN_IMGUI_FILTER" ]; then
    RUN_IMGUI_MODE=true
fi

if [ "$RUN_IMGUI_MODE" = true ] && [ -n "$TEST_NAME" ]; then
    echo "ERROR: Positional suite test names cannot be combined with ImGui test options"
    exit 2
fi

RESULTS_DIR="$PROJECT_DIR/tests/results"
RESULTS_FILE="$RESULTS_DIR/last_run.txt"
XCODE_PROJECT="$PROJECT_DIR/platform/MacOS/c64d.xcodeproj"
BUILD_DIR="$PROJECT_DIR/platform/MacOS/DerivedData"
APP_BINARY=""
APP_BINARY_MTIME=0

select_newest_binary() {
    local candidate=""
    local mtime=0

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

# Ensure results directory exists
mkdir -p "$RESULTS_DIR"

if [ ${#EXTRA_APP_ARGS[@]} -gt 0 ]; then
    for extra_arg in "${EXTRA_APP_ARGS[@]}"; do
        for reserved_flag in "${RESERVED_APP_FLAGS[@]}"; do
            if [ "$extra_arg" = "$reserved_flag" ]; then
                echo "ERROR: App options after -- cannot include reserved runner flags: $reserved_flag"
                exit 2
            fi
        done
    done

    for ((i = 0; i < ${#EXTRA_APP_ARGS[@]}; i++)); do
        if [ "${EXTRA_APP_ARGS[$i]}" = "--layouts-file" ]; then
            if [ $((i + 1)) -ge ${#EXTRA_APP_ARGS[@]} ]; then
                echo "ERROR: --layouts-file requires a path"
                exit 2
            fi

            FORWARDED_LAYOUTS_FILE_COUNT=$((FORWARDED_LAYOUTS_FILE_COUNT + 1))
            FORWARDED_LAYOUTS_FILE="${EXTRA_APP_ARGS[$((i + 1))]}"
        fi
    done
fi

if [ "$FORWARDED_LAYOUTS_FILE_COUNT" -gt 1 ]; then
    echo "ERROR: Forwarded app options can include --layouts-file at most once"
    exit 2
fi

if [ -n "$LAYOUTS_FIXTURE" ]; then
    if [ -n "$FORWARDED_LAYOUTS_FILE" ]; then
        echo "ERROR: Use either --layouts-fixture or forwarded --layouts-file, not both"
        exit 2
    fi

    if [[ "$LAYOUTS_FIXTURE" = /* ]]; then
        LAYOUTS_FIXTURE_PATH="$LAYOUTS_FIXTURE"
    else
        LAYOUTS_FIXTURE_PATH="$PROJECT_DIR/$LAYOUTS_FIXTURE"
    fi

    if [ ! -f "$LAYOUTS_FIXTURE_PATH" ]; then
        echo "ERROR: Layout fixture not found: $LAYOUTS_FIXTURE"
        exit 2
    fi

    copy_layouts_file_to_tmp "$LAYOUTS_FIXTURE_PATH"
    if [ "$LAYOUTS_FIXTURE_PATH" = "$KNOWN_LAYOUTS_FIXTURE_PATH" ]; then
        COPIED_LAYOUTS_MATCHES_KNOWN_FIXTURE=true
    fi
elif [ -n "$FORWARDED_LAYOUTS_FILE" ]; then
    if [[ "$FORWARDED_LAYOUTS_FILE" = /* ]]; then
        FORWARDED_LAYOUTS_FILE_PATH="$FORWARDED_LAYOUTS_FILE"
    else
        FORWARDED_LAYOUTS_FILE_PATH="$PROJECT_DIR/$FORWARDED_LAYOUTS_FILE"
    fi

    if [ ! -f "$FORWARDED_LAYOUTS_FILE_PATH" ]; then
        echo "ERROR: Layouts file not found: $FORWARDED_LAYOUTS_FILE"
        exit 2
    fi

    copy_layouts_file_to_tmp "$FORWARDED_LAYOUTS_FILE_PATH"
    if [ "$FORWARDED_LAYOUTS_FILE_PATH" = "$KNOWN_LAYOUTS_FIXTURE_PATH" ]; then
        COPIED_LAYOUTS_MATCHES_KNOWN_FIXTURE=true
    fi

    REWRITTEN_APP_ARGS=()
    replaced_layouts_file=false
    for ((i = 0; i < ${#EXTRA_APP_ARGS[@]}; i++)); do
        arg="${EXTRA_APP_ARGS[$i]}"
        if [ "$arg" = "--layouts-file" ] && [ "$replaced_layouts_file" = false ]; then
            REWRITTEN_APP_ARGS+=("--layouts-file" "$COPIED_LAYOUTS_FILE")
            i=$((i + 1))
            replaced_layouts_file=true
            continue
        fi
        REWRITTEN_APP_ARGS+=("$arg")
    done
    EXTRA_APP_ARGS=("${REWRITTEN_APP_ARGS[@]}")
fi

# Step 1: Build
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

# Step 2: Remove old results
rm -f "$RESULTS_FILE"

# Step 3: Find the built binary
select_newest_binary \
    "$BUILD_DIR"/Build/Products/Release/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$BUILD_DIR"/Build/Products/Debug/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$BUILD_DIR"/*/Build/Products/Release/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
    "$BUILD_DIR"/*/Build/Products/Debug/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger"

if [ -z "$APP_BINARY" ]; then
    select_newest_binary \
        "$HOME"/Library/Developer/Xcode/DerivedData/*/Build/Products/Release/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger" \
        "$HOME"/Library/Developer/Xcode/DerivedData/*/Build/Products/Debug/"Retro Debugger.app"/Contents/MacOS/"Retro Debugger"
fi

if [ -z "$APP_BINARY" ]; then
    echo "ERROR: Could not find Retro Debugger binary. Build first or check DerivedData path."
    exit 2
fi

echo "=== Using binary: $APP_BINARY ==="

# Step 4: Run the binary with test flags.
# Extra app arguments can be forwarded after --, for example layout files.
# Change to project directory so results file path is correct
cd "$PROJECT_DIR"

APP_ARGS=(--log-dir "$LOG_DIR")
if [ "$VISIBLE" = false ]; then
    if [ "$RUN_IMGUI_MODE" = true ]; then
        :
    else
    APP_ARGS=(--headless "${APP_ARGS[@]}")
    fi
fi

if [ -n "$COPIED_LAYOUTS_FILE" ]; then
    export C64D_TEST_EXPECTED_LAYOUTS_FILE="$COPIED_LAYOUTS_FILE"
    if [ "$COPIED_LAYOUTS_MATCHES_KNOWN_FIXTURE" = true ]; then
        export C64D_TEST_EXPECTED_LAYOUTS_FIXTURE=1
    else
        unset C64D_TEST_EXPECTED_LAYOUTS_FIXTURE
    fi
    if [ -z "$FORWARDED_LAYOUTS_FILE" ]; then
        APP_ARGS=(--layouts-file "$COPIED_LAYOUTS_FILE" "${APP_ARGS[@]}")
    fi
else
    unset C64D_TEST_EXPECTED_LAYOUTS_FILE
    unset C64D_TEST_EXPECTED_LAYOUTS_FIXTURE
fi

if [ ${#EXTRA_APP_ARGS[@]} -gt 0 ]; then
    APP_ARGS=("${EXTRA_APP_ARGS[@]}" "${APP_ARGS[@]}")
fi

if [ "$RUN_IMGUI_ALL" = true ]; then
    echo "=== Running all ImGui UI tests ==="
    APP_ARGS+=(--run-tests --exit-after-tests)
elif [ -n "$RUN_IMGUI_FILTER" ]; then
    echo "=== Running ImGui UI tests matching: $RUN_IMGUI_FILTER ==="
    APP_ARGS+=(--run-imgui-test "$RUN_IMGUI_FILTER" --exit-after-tests)
elif [ -n "$TEST_NAME" ]; then
    echo "=== Running test: $TEST_NAME ==="
    APP_ARGS+=(--run-test "$TEST_NAME" --exit-after-tests)
else
    echo "=== Running all suite tests ==="
    APP_ARGS+=(--run-suite --exit-after-tests)
    # Tests that know they're flaky in full-suite context can check
    # this and skip. Single-test mode does not set it.
    export C64D_IN_SUITE=1
fi

"$APP_BINARY" "${APP_ARGS[@]}" &

APP_PID=$!

# Step 5: Wait with timeout
ELAPSED=0
TIMED_OUT=false
while kill -0 "$APP_PID" 2>/dev/null; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))
    if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
        echo "TIMEOUT: Test did not complete within ${TIMEOUT}s"
        kill "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
        TIMED_OUT=true
        break
    fi
done

APP_STATUS=0
if [ "$TIMED_OUT" = false ]; then
    wait "$APP_PID" 2>/dev/null || APP_STATUS=$?
fi

# Step 6: Check results
if [ ! -f "$RESULTS_FILE" ]; then
    if [ "$TIMED_OUT" = true ]; then
        echo "ERROR: Timed out AND no results file found. The app likely crashed or never ran tests."
        exit 3
    fi
    echo "ERROR: Results file not found at $RESULTS_FILE"
    echo "The application may have crashed before writing results."
    exit 1
fi

echo ""
echo "=== Test Results ==="
cat "$RESULTS_FILE"
echo ""

# Check the RESULT line for pass/fail
RESULT_LINE=$(grep "^RESULT:" "$RESULTS_FILE" || true)
if [ -z "$RESULT_LINE" ]; then
    echo "ERROR: No RESULT line found in results file"
    exit 1
fi

# Extract passed/total
PASSED=$(echo "$RESULT_LINE" | sed 's/RESULT: \([0-9]*\)\/.*/\1/')
TOTAL=$(echo "$RESULT_LINE" | sed 's/RESULT: [0-9]*\/\([0-9]*\).*/\1/')

if [ "$PASSED" = "$TOTAL" ] && [ "$TOTAL" != "0" ]; then
	if [ "$TIMED_OUT" = true ]; then
		echo "APPLICATION TIMED OUT after passing-looking results file"
		exit 1
	fi
    if [ "$TIMED_OUT" = false ] && [ "$APP_STATUS" != "0" ]; then
        echo "APPLICATION FAILED (exit $APP_STATUS) despite passing-looking results file"
        exit 1
    fi
    echo "ALL TESTS PASSED ($PASSED/$TOTAL)"
    exit 0
else
    echo "TESTS FAILED ($PASSED/$TOTAL passed)"
    exit 1
fi
