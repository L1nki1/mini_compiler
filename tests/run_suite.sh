#!/usr/bin/env bash

set -u

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mini_cc="${1:-${MINI_CC:-}}"
out_dir="${2:-${TEST_OUT_DIR:-/tmp/mini_compiler_tests}}"
target="${MINI_TARGET:-x86_64-pc-linux-gnu}"

if [[ -z "$mini_cc" ]]; then
    echo "usage: bash tests/run_suite.sh /path/to/mini-cc [output-dir]" >&2
    echo "or set MINI_CC=/path/to/mini-cc" >&2
    exit 2
fi

if [[ ! -x "$mini_cc" ]]; then
    echo "test runner error: compiler is not executable: $mini_cc" >&2
    exit 2
fi

mkdir -p "$out_dir"
rm -rf "$out_dir/valid" "$out_dir/invalid" "$out_dir/logs"
mkdir -p "$out_dir/valid" "$out_dir/invalid" "$out_dir/logs"

valid_ok=0
valid_total=0
invalid_ok=0
invalid_total=0
failures=0

echo "Mini compiler self-check"
echo "target: $target"
echo

echo "[valid inputs: must compile]"
for source in "$repo_root"/tests/valid/*.mc; do
    name="$(basename "$source" .mc)"
    object="$out_dir/valid/$name.o"
    ir="$out_dir/valid/$name.ll"
    log="$out_dir/logs/$name.valid.log"
    valid_total=$((valid_total + 1))

    if "$mini_cc" "$source" -o "$object" --emit-ir "$ir" --target="$target" >"$log" 2>&1; then
        printf "  OK   %-24s -> object + llvm-ir\n" "$name"
        valid_ok=$((valid_ok + 1))
    else
        printf "  FAIL %-24s -> expected successful compile\n" "$name"
        sed 's/^/       /' "$log"
        failures=$((failures + 1))
    fi
done

echo
echo "[invalid inputs: must be rejected]"
for source in "$repo_root"/tests/invalid/*.mc; do
    name="$(basename "$source" .mc)"
    object="$out_dir/invalid/$name.o"
    ir="$out_dir/invalid/$name.ll"
    log="$out_dir/logs/$name.invalid.log"
    invalid_total=$((invalid_total + 1))

    if "$mini_cc" "$source" -o "$object" --emit-ir "$ir" --target="$target" >"$log" 2>&1; then
        printf "  FAIL %-24s -> invalid program was accepted\n" "$name"
        failures=$((failures + 1))
    else
        reason="diagnostic"
        if grep -q "semantic error" "$log"; then
            reason="semantic error"
        elif grep -q "syntax error" "$log"; then
            reason="syntax error"
        elif grep -q "lexical error" "$log"; then
            reason="lexical error"
        fi
        printf "  OK   %-24s -> rejected (%s)\n" "$name" "$reason"
        invalid_ok=$((invalid_ok + 1))
    fi
done

echo
printf "summary: valid %d/%d, invalid %d/%d\n" \
    "$valid_ok" "$valid_total" "$invalid_ok" "$invalid_total"

if [[ "$failures" -ne 0 ]]; then
    echo "result: FAILED"
    exit 1
fi

echo "result: PASSED"
