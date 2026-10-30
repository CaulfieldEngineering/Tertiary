#!/usr/bin/env bash
# woo_product.sh: release checks on the plugin's WooCommerce product (Serial Numbers plugin)
# ==========================================================================================
# [Claude 2026-10-29] Joe: the "Software version" field is what the plugin's version_check route returns
# ("new version available"), so it changes only once the new installers are live. And a product set to
# "Generator rule" with no generator gives buyers no key, so a release stops before publishing.
#
#   woo_product.sh check-key-source     the product sells serial keys and has a working key source
#   woo_product.sh set-version <x.y.z>  sets the Software version field, reads it back, fails if it differs
#
# Environment: WORDPRESS_SITE, WORDPRESS_USERNAME, WORDPRESS_APPLICATION_PASSWORD (the existing secrets),
#              WOO_PRODUCT_ID (wWOO_PRODUCT_ID in config.cmake), WARN_ONLY=1 to report problems as warnings.
#
# Meta keys, from the Serial Numbers plugin source (wc-serial-numbers, includes/Admin/Products.php and
# includes/RestAPI.php, where version_check returns get_post_meta(product, '_software_version')), checked
# against the live product 45 on 2026-10-30:
#   _is_serial_number = yes        serial keys are on for this product
#   _serial_key_source             auto_generated (Pro: "Auto generated key", what Tertiary uses since
#                                  2026-10-30, with _serial_number_key_prefix WLA- and _activation_limit 5),
#                                  generator_rule (Pro: needs _generator_id), custom_source (keys added by hand)
#   _generator_id                  the generator a generator_rule product uses (0 or empty = none chosen)
#   _software_version              the Software version field
# A product with no key source, or one this script doesn't know, fails: buyers might get no key.
# Tests: .github/scripts/test_woo_product.sh (a fake curl, no network).
# ==========================================================================================
set -euo pipefail

: "${WORDPRESS_SITE:?WORDPRESS_SITE is not set}"
: "${WORDPRESS_USERNAME:?WORDPRESS_USERNAME is not set}"
: "${WORDPRESS_APPLICATION_PASSWORD:?WORDPRESS_APPLICATION_PASSWORD is not set}"
: "${WOO_PRODUCT_ID:?WOO_PRODUCT_ID is not set (wWOO_PRODUCT_ID in config.cmake)}"

VERSION_KEY="_software_version"
API="${WORDPRESS_SITE%/}/wp-json/wc/v3/products/${WOO_PRODUCT_ID}"
AUTH="$WORDPRESS_USERNAME:$WORDPRESS_APPLICATION_PASSWORD"

fail() {
  if [ "${WARN_ONLY:-0}" = "1" ]; then
    echo "::warning::$*"
    exit 0
  fi
  echo "::error::$*"
  exit 1
}

# api METHOD [json body]: prints the body; returns 1 unless 2xx and a product came back, with the reason in
# $ERR_FILE (it runs inside $(...), so it can't stop the script itself; callers do: P=$(api GET) || fail_api)
ERR_FILE=$(mktemp)
fail_api() { fail "$(cat "$ERR_FILE")"; }
api() {
  local method=$1 body=${2:-} out code
  out=$(mktemp)
  if [ -n "$body" ]; then
    code=$(curl -sS -o "$out" -w '%{http_code}' -X "$method" -u "$AUTH" -H "Content-Type: application/json" -d "$body" "$API") || code=000
  else
    code=$(curl -sS -o "$out" -w '%{http_code}' -X "$method" -u "$AUTH" "$API") || code=000
  fi
  if [ "${code:0:1}" != "2" ]; then
    echo "WooCommerce API $method $API answered HTTP $code: $(head -c 500 "$out")" > "$ERR_FILE"
    return 1
  fi
  if ! jq -e 'type == "object" and has("meta_data")' "$out" >/dev/null 2>&1; then
    echo "WooCommerce API $method $API did not return a product: $(head -c 500 "$out")" > "$ERR_FILE"
    return 1
  fi
  cat "$out"
}

meta() {   # meta <product json> <key>: the value as text, empty if absent
  jq -r --arg k "$2" '[.meta_data[]? | select(.key == $k) | .value][0] // empty | if type == "string" then . else tojson end' <<< "$1"
}

log_keys() {   # log_keys <product json> <regex>
  jq -r --arg re "$2" '.meta_data[]? | select(.key | test($re; "i")) | "    \(.key) = \(.value | if type == "string" then . else tojson end)"' <<< "$1"
}

case "${1:-}" in
  check-key-source)
    P=$(api GET) || fail_api
    echo "Product $WOO_PRODUCT_ID: $(jq -r .name <<< "$P")"
    echo "  Serial Numbers meta on the product:"
    log_keys "$P" 'serial|generator|key_source|software'

    [ "$(meta "$P" _is_serial_number)" = "yes" ] || fail "Product $WOO_PRODUCT_ID does not sell serial keys (_is_serial_number is not 'yes'), so buyers would get no key."

    SOURCE=$(meta "$P" _serial_key_source)
    case "$SOURCE" in
      auto_generated)
        echo "✓ Key source: auto generated key (prefix '$(meta "$P" _serial_number_key_prefix)', activation limit '$(meta "$P" _activation_limit)')"
        ;;
      generator_rule)
        GEN=$(meta "$P" _generator_id)
        if [ -z "$GEN" ] || [ "$GEN" = "0" ]; then
          fail "Product $WOO_PRODUCT_ID is set to 'Generator rule' but no generator is chosen (_generator_id is '${GEN}'), so purchases get no key. Pick the generator on the product, or switch it to 'Auto generated key', then re-run."
        fi
        echo "✓ Key source: generator rule, generator $GEN"
        ;;
      custom_source)
        echo "✓ Key source: keys added by hand (custom_source). The stock of unused keys is not checked here."
        ;;
      "")
        fail "Product $WOO_PRODUCT_ID has no key source (_serial_key_source is empty or missing), so purchases may get no key. Set it to 'Auto generated key' in the product's Serial Numbers tab, then re-run."
        ;;
      *)
        fail "Product $WOO_PRODUCT_ID has a key source this check doesn't know ('$SOURCE'). Check the product delivers keys, then add the source to woo_product.sh."
        ;;
    esac
    ;;

  set-version)
    VERSION=${2:?usage: woo_product.sh set-version <version>}
    P=$(api GET) || fail_api
    if ! jq -e --arg k "$VERSION_KEY" 'any(.meta_data[]?; .key == $k)' <<< "$P" >/dev/null; then
      echo "Meta keys on product $WOO_PRODUCT_ID that look like a version:"
      log_keys "$P" 'version'
      fail "Product $WOO_PRODUCT_ID has no $VERSION_KEY meta, so the Software version field can't be found. Set it once by hand in the product's Serial Numbers tab, then re-run."
    fi
    echo "Software version was: $(meta "$P" "$VERSION_KEY")"

    BODY=$(jq -n --arg k "$VERSION_KEY" --arg v "$VERSION" '{meta_data: [{key: $k, value: $v}]}')
    api PUT "$BODY" >/dev/null || fail_api

    P=$(api GET) || fail_api
    NOW=$(meta "$P" "$VERSION_KEY")
    [ "$NOW" = "$VERSION" ] || fail "Software version on product $WOO_PRODUCT_ID reads '$NOW' after the update, not '$VERSION'."
    echo "✓ Software version is now $NOW"
    ;;

  *)
    echo "usage: woo_product.sh check-key-source | set-version <version>" >&2
    exit 2
    ;;
esac
