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
# includes/RestAPI.php, where version_check returns get_post_meta(product, '_software_version')):
#   _is_serial_number = yes        serial keys are on for this product
#   _serial_key_source             custom_source (keys added by hand) or generator_rule (Pro)
#   _software_version              the Software version field
# The generator a "generator_rule" product uses is stored by the Pro plugin, whose source isn't public:
# the check looks for meta keys containing "generator", logs them, and fails if none holds a value.
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
      generator_rule)
        GEN=$(jq -r '[.meta_data[]? | select(.key | test("generator"; "i")) | .value
                       | select(. != null and . != "" and . != "0" and . != 0 and . != [] and . != {})] | length' <<< "$P")
        if [ "$GEN" = "0" ]; then
          fail "Product $WOO_PRODUCT_ID is set to 'Generator rule' but no generator is chosen (no meta key containing 'generator' has a value), so purchases get no key. Pick the generator on the product, then re-run."
        fi
        echo "✓ Key source: generator rule, with a generator set"
        ;;
      custom_source|"")
        echo "✓ Key source: keys added by hand (custom_source). The stock of unused keys is not checked here."
        ;;
      *)
        echo "✓ Key source: $SOURCE"
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
