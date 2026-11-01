#!/bin/bash
# [Claude 2026-10-31] Shared helpers for scripts/mac/*.sh and scripts/web/*.sh. Source it; it sets ROOT and the
# config.cmake values (wPLUGIN_NAME, wPROJECT_VERSION, wFORMATS, ...) and reports each step the way tests/ does
# (scripts/lib/report.py: progress.jsonl and <ID>.json). Nothing here needs sudo or changes the system.
#
#   . "$(dirname "$0")/../lib/common.sh"
#   step_begin B-01 "Plug-in"
#   ... work, logging with: run_logged cmake --build ...
#   step_end PASS "built ..."          # exits: 0 PASS, 1 FAIL, 2 SKIP, 3 PLANNED (on GitHub only FAIL is non-zero)

LIB_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd -- "$LIB_DIR/../.." && pwd)"
PYTHON="${WLA_TEST_PYTHON:-}"
if [ -z "$PYTHON" ]; then
  if command -v python3 >/dev/null 2>&1 && python3 -c 'import sys' >/dev/null 2>&1; then PYTHON=python3; else PYTHON=python; fi
fi
case "$(uname -s)" in Darwin) PLATFORM=mac ;; MINGW*|MSYS*|CYGWIN*) PLATFORM=win ;; *) PLATFORM=linux ;; esac

# config.cmake: every set(wNAME value...) line becomes a shell variable (quotes removed, words joined by one space)
while IFS= read -r line; do
  key=$(printf '%s' "$line" | sed -E 's/^[[:space:]]*set\((w[A-Za-z0-9_]+)[[:space:]]+.*$/\1/')
  val=$(printf '%s' "$line" | sed -E 's/^[[:space:]]*set\(w[A-Za-z0-9_]+[[:space:]]+(.*)\)[[:space:]]*(#.*)?$/\1/' | tr -d '"' | tr -s ' \t' ' ' | sed -E 's/ +$//')
  [ -n "$key" ] && [ "$key" != "$line" ] && printf -v "$key" '%s' "$val"
done < <(grep -E '^[[:space:]]*set\(w' "$ROOT/config.cmake")
# ${wPLUGIN_NAME} inside other values (config.cmake uses it in paths)
for v in $(compgen -v | grep '^w'); do printf -v "$v" '%s' "${!v//\$\{wPLUGIN_NAME\}/$wPLUGIN_NAME}"; done

DRY_RUN=0
STEP_ID=""
STEP_LOG=""
STEP_ARTIFACTS=()

say() { printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }
in_ci() { [ "${GITHUB_ACTIONS:-}" = "true" ]; }
on_tag() { [[ "${GITHUB_REF:-}" == refs/tags/v* ]]; }
warn() { if in_ci; then echo "::warning::$*"; else say "WARNING: $*"; fi; }
err() { if in_ci; then echo "::error::$*"; else say "ERROR: $*"; fi; }

step_begin() {
  STEP_ID="$1"; local title="$2"
  if [ -z "${WLA_BUILD_RESULTS:-}" ]; then
    WLA_BUILD_RESULTS="$("$PYTHON" "$LIB_DIR/report.py" folder)" || { echo "cannot create a results folder"; exit 1; }
    export WLA_BUILD_RESULTS
  fi
  mkdir -p "$WLA_BUILD_RESULTS/$STEP_ID-artifacts"
  STEP_LOG="$WLA_BUILD_RESULTS/$STEP_ID-artifacts/$(printf '%s' "$STEP_ID" | tr 'A-Z' 'a-z').log"
  : > "$STEP_LOG"
  STEP_ARTIFACTS=("$STEP_LOG")
  "$PYTHON" "$LIB_DIR/report.py" begin --id "$STEP_ID" --title "$title" --results "$WLA_BUILD_RESULTS"
  say "$STEP_ID $title$([ "$DRY_RUN" = 1 ] && echo ' (dry run)')"
}

# step_end STATUS DETAIL: records the result and exits with its code
step_end() {
  local status="$1" detail="$2" a args=()
  for a in "${STEP_ARTIFACTS[@]}"; do [ -e "$a" ] && args+=(--artifact "$a"); done
  "$PYTHON" "$LIB_DIR/report.py" end --id "$STEP_ID" --status "$status" --detail "$detail" --results "$WLA_BUILD_RESULTS" "${args[@]}"
  case "$status" in
    PASS) exit 0 ;;
    FAIL) exit 1 ;;
    SKIP) in_ci && exit 0; exit 2 ;;
    *) in_ci && exit 0; exit 3 ;;
  esac
}

fail() { err "$*"; step_end FAIL "$*"; }

# run_logged CMD...: runs it, output to the console and the step log; returns its exit code.
# In a dry run it only prints the command.
run_logged() {
  printf '> %s\n' "$*" | tee -a "$STEP_LOG"
  [ "$DRY_RUN" = 1 ] && return 0
  set -o pipefail
  "$@" 2>&1 | tee -a "$STEP_LOG"
  local code=${PIPESTATUS[0]}
  set +o pipefail
  return "$code"
}

tool_path() { "$PYTHON" "$LIB_DIR/tools.py" path "$@"; }

# The CMake to use: WLA_TEST_CMAKE (the Mac keeps cmake under ~/Library/Python), else cmake on PATH
cmake_exe() { printf '%s' "${WLA_TEST_CMAKE:-cmake}"; }

# BUILD_DEMO from a configured build folder's cache (the release builds' ".D" suffix follows it)
build_suffix() {
  if grep -q '^BUILD_DEMO:BOOL=ON' "$1/CMakeCache.txt" 2>/dev/null; then printf '.D'; fi
}

has_format() { case " $wFORMATS " in *" $1 "*) return 0 ;; *) return 1 ;; esac; }

# A temporary keychain holding one base64 .p12, exactly as the release workflow has always done it.
# keychain_open NAME BASE64 PASSWORD KEYCHAIN_PASSWORD ; keychain_close
keychain_open() {
  local name="$1" b64="$2" pass="$3" kpass="$4" tmp="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
  KEYCHAIN_PATH="$tmp/$name.keychain-db"; CERT_P12="$tmp/$name-cert.p12"
  security create-keychain -p "$kpass" "$KEYCHAIN_PATH"
  security set-keychain-settings -lut 21600 "$KEYCHAIN_PATH"
  security unlock-keychain -p "$kpass" "$KEYCHAIN_PATH"
  echo -n "$b64" | base64 --decode > "$CERT_P12"
  security import "$CERT_P12" -P "$pass" -A -t cert -f pkcs12 -k "$KEYCHAIN_PATH"
  security list-keychains -d user -s "$KEYCHAIN_PATH"
  security default-keychain -s "$KEYCHAIN_PATH"
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$kpass" "$KEYCHAIN_PATH"
}
keychain_close() {
  [ -n "${KEYCHAIN_PATH:-}" ] || return 0
  security default-keychain -s login.keychain
  security delete-keychain "$KEYCHAIN_PATH"
  rm -f "$CERT_P12"
  KEYCHAIN_PATH=""
}
