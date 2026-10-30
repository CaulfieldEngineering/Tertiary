#!/usr/bin/env bash
# test_woo_product.sh: tests for woo_product.sh, with a fake curl (no network, nothing on the store changes)
# ==========================================================================================
# [Claude 2026-10-30] Run: bash .github/scripts/test_woo_product.sh   (needs jq; CI runs it in preflight)
# Each case gives the fake store a product (meta as key=value pairs) and checks the exit code and message.
# ==========================================================================================
set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/bin"

# The fake curl: GET returns $WORK/product.json, PUT merges its meta_data into it (unless IGNORE_PUT=1).
# HTTP_CODE overrides the status. Prints the -w status code like the real one.
cat > "$WORK/bin/curl" <<'FAKE'
#!/usr/bin/env bash
out="" method=GET body=""
while [ $# -gt 0 ]; do
  case "$1" in
    -o) out=$2; shift 2 ;;
    -X) method=$2; shift 2 ;;
    -d) body=$2; shift 2 ;;
    -w|-u|-H) shift 2 ;;
    *) shift ;;
  esac
done
code=${HTTP_CODE:-200}
if [ "$method" = PUT ] && [ "${IGNORE_PUT:-0}" != 1 ] && [ "$code" = 200 ]; then
  jq --argjson b "$body" '
    reduce $b.meta_data[] as $m (.;
      if any(.meta_data[]; .key == $m.key)
      then .meta_data |= map(if .key == $m.key then .value = $m.value else . end)
      else .meta_data += [$m] end)' "$FAKE_STORE/product.json" > "$FAKE_STORE/p.tmp" && mv "$FAKE_STORE/p.tmp" "$FAKE_STORE/product.json"
fi
if [ "$code" = 200 ]; then cat "$FAKE_STORE/product.json" > "$out"; else echo '{"code":"woocommerce_rest_cannot_view"}' > "$out"; fi
printf '%s' "$code"
FAKE
chmod +x "$WORK/bin/curl"

export PATH="$WORK/bin:$PATH" FAKE_STORE="$WORK"
export WORDPRESS_SITE=https://store.invalid WORDPRESS_USERNAME=u WORDPRESS_APPLICATION_PASSWORD=p WOO_PRODUCT_ID=45

product() {   # product key=value ...: writes the fake store's product
  local args=() kv
  for kv in "$@"; do args+=(--arg "${kv%%=*}" "${kv#*=}"); done
  jq -n '{id: 45, name: "Tertiary", meta_data: ($ARGS.named | to_entries | map({key, value}))}' "${args[@]}" > "$WORK/product.json"
}

PASS=0 FAILED=0
check() {   # check <name> <want exit> <want text> -- <command...>
  local name=$1 want=$2 text=$3; shift 4
  local got out
  out=$("$@" 2>&1); got=$?
  if [ "$got" = "$want" ] && grep -qF -- "$text" <<< "$out"; then
    echo "  pass  $name"; PASS=$((PASS+1))
  else
    echo "  FAIL  $name (exit $got, wanted $want; wanted text: $text)"; sed 's/^/        /' <<< "$out"; FAILED=$((FAILED+1))
  fi
}
S="$HERE/woo_product.sh"

echo "woo_product.sh check-key-source"
product _is_serial_number=yes _serial_key_source=auto_generated _serial_number_key_prefix=WLA- _activation_limit=5 _generator_id=4763
check "auto generated key (Tertiary today)"     0 "auto generated key (prefix 'WLA-'"   -- bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=generator_rule _generator_id=4763
check "generator rule with a generator"         0 "generator 4763"                      -- bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=generator_rule _generator_id=0
check "generator rule, generator 0"             1 "no generator is chosen"              -- bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=generator_rule
check "generator rule, no generator meta"       1 "no generator is chosen"              -- bash "$S" check-key-source
product _is_serial_number=yes
check "no key source"                           1 "has no key source"                   -- bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=
check "empty key source"                        1 "has no key source"                   -- bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=something_new
check "unknown key source"                      1 "doesn't know ('something_new')"      -- bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=custom_source
check "keys added by hand"                      0 "custom_source"                       -- bash "$S" check-key-source
product _is_serial_number=no _serial_key_source=auto_generated
check "serial keys off"                         1 "does not sell serial keys"           -- bash "$S" check-key-source
product _is_serial_number=yes
check "staging: a problem is only a warning"    0 "::warning::"                         -- env WARN_ONLY=1 bash "$S" check-key-source
product _is_serial_number=yes _serial_key_source=auto_generated
check "store answers 401"                       1 "answered HTTP 401"                   -- env HTTP_CODE=401 bash "$S" check-key-source

echo "woo_product.sh set-version"
product _is_serial_number=yes _serial_key_source=auto_generated _software_version=
check "sets the version and reads it back"      0 "Software version is now 1.1.0"       -- bash "$S" set-version 1.1.0
check "the store really holds it"               0 '"1.1.0"'                             -- jq -c '.meta_data[] | select(.key == "_software_version") | .value' "$WORK/product.json"
product _is_serial_number=yes _serial_key_source=auto_generated _software_version=1.0.2
check "store ignores the update: fails"         1 "reads '1.0.2' after the update"      -- env IGNORE_PUT=1 bash "$S" set-version 1.1.0
product _is_serial_number=yes _serial_key_source=auto_generated
check "no Software version field: fails"        1 "has no _software_version meta"       -- bash "$S" set-version 1.1.0
check "no version given: usage"                 1 "usage"                               -- bash "$S" set-version

echo "$PASS passed, $FAILED failed"
[ "$FAILED" = 0 ]
