#!/bin/bash
# A throwaway Mac user account for the "person with a Mac installs it and opens it in their DAW" test.
# The account is an admin, like most customers, so the installer asks for its password the way a customer's would.
#
#   sudo scripts/mac/customer-test-account.sh create   # make the account (password from $CUSTOMER_TEST_PASSWORD or stdin)
#   sudo scripts/mac/customer-test-account.sh reset    # after a test run: remove the account, its files and every
#                                                      # system-wide copy of the product, then make the account again
#   sudo scripts/mac/customer-test-account.sh remove   # remove it all and leave no account
#   scripts/mac/customer-test-account.sh status        # what is there now
#
# Removing the product clears what the installer writes: the AU and VST3 in /Library/Audio/Plug-Ins, the app in
# /Applications and the installer receipts. The owner's own account and its settings are never touched.
set -euo pipefail

USER_NAME="${CUSTOMER_TEST_USER:-customertest}"
FULL_NAME="${CUSTOMER_TEST_FULL_NAME:-Customer Test}"
PRODUCT="${CUSTOMER_TEST_PRODUCT:-Tertiary}"

die() { echo "customer-test-account: $*" >&2; exit 1; }
need_root() { [ "$(id -u)" -eq 0 ] || die "run with sudo"; }
exists() { dscl . -read "/Users/$USER_NAME" >/dev/null 2>&1; }

product_paths() {
  local p
  for p in "/Library/Audio/Plug-Ins/Components/$PRODUCT.component" "/Library/Audio/Plug-Ins/VST3/$PRODUCT.vst3" \
           "/Applications/$PRODUCT.app"; do
    [ -e "$p" ] && echo "$p"
  done
  return 0
}
product_receipts() { pkgutil --pkgs | grep -i "\.$PRODUCT\." || true; }

status() {
  if exists; then echo "account: $USER_NAME ($FULL_NAME), home $(dscl . -read "/Users/$USER_NAME" NFSHomeDirectory | awk '{print $2}')"
  else echo "account: none"; fi
  echo "installed copies: $(product_paths | tr '\n' ' ')"
  echo "installer receipts: $(product_receipts | tr '\n' ' ')"
}

remove_product() {
  local p r
  while read -r p; do [ -n "$p" ] && rm -rf "$p" && echo "removed $p"; done < <(product_paths)
  while read -r r; do [ -n "$r" ] && pkgutil --forget "$r" >/dev/null && echo "forgot receipt $r"; done < <(product_receipts)
  return 0
}

remove_account() {
  if exists; then
    pkill -u "$USER_NAME" 2>/dev/null || true
    sleep 2
    sysadminctl -deleteUser "$USER_NAME" 2>&1 | grep -v "^$" || true
    exists && die "could not delete $USER_NAME"
    echo "deleted account $USER_NAME and its home folder"
  fi
  rm -rf "/Users/$USER_NAME" "/Users/Deleted Users/$USER_NAME"*.dmg 2>/dev/null || true
}

create_account() {
  exists && die "$USER_NAME already exists (use reset)"
  local pw="${CUSTOMER_TEST_PASSWORD:-}"
  [ -n "$pw" ] || read -r pw
  [ -n "$pw" ] || die "no password given"
  sysadminctl -addUser "$USER_NAME" -fullName "$FULL_NAME" -password "$pw" -admin 2>&1 | grep -v "^$" || true
  exists || die "could not create $USER_NAME"
  createhomedir -c -u "$USER_NAME" >/dev/null 2>&1 || true
  echo "created account $USER_NAME ($FULL_NAME), admin"
}

case "${1:-status}" in
  status) status ;;
  create) need_root; create_account; status ;;
  reset)  need_root; pw="${CUSTOMER_TEST_PASSWORD:-}"; [ -n "$pw" ] || read -r pw; export CUSTOMER_TEST_PASSWORD="$pw"
          remove_account; remove_product; create_account; status ;;
  remove) need_root; remove_account; remove_product; status ;;
  *) die "unknown command '$1' (create, reset, remove, status)" ;;
esac
