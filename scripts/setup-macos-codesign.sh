#!/usr/bin/env bash
#
# One-time setup: create a stable, self-signed code-signing identity ("SculptSampler Dev") in your
# login keychain. After this, every Standalone build is signed with the same identity (see
# cmake/CodesignStandalone.cmake), so macOS remembers the folders you grant the app access to
# instead of re-prompting after each rebuild.
#
# Why this is needed: the default build is *ad-hoc* signed. macOS keys file-access permissions to
# the exact binary hash of an ad-hoc app, and that hash changes on every rebuild -- so the OS treats
# each build as a brand-new app and forgets what you authorized. A stable signature avoids that.
#
# Safe and reversible. It only ADDS a local dev certificate. Remove it any time with:
#   security delete-identity -c "SculptSampler Dev"
#
set -euo pipefail

ID="SculptSampler Dev"
KEYCHAIN="$HOME/Library/Keychains/login.keychain-db"

if security find-identity -p codesigning | grep -q "$ID"; then
    echo "Identity '$ID' already exists - nothing to do."
    echo "Rebuild the Standalone target and it will be signed with it."
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
PW="sculptdev"

echo "Creating self-signed code-signing certificate '$ID'..."
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "$TMP/key.pem" -out "$TMP/cert.pem" -days 3650 \
    -subj "/CN=$ID" \
    -addext "basicConstraints=critical,CA:false" \
    -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=critical,codeSigning" >/dev/null 2>&1

# -legacy + a non-empty password: required for Apple's `security import` to read a PKCS12 produced
# by OpenSSL 3.x (its modern PBE/MAC algorithms otherwise fail with "MAC verification failed").
openssl pkcs12 -export -legacy \
    -inkey "$TMP/key.pem" -in "$TMP/cert.pem" -out "$TMP/id.p12" \
    -passout "pass:$PW" -name "$ID" >/dev/null 2>&1

# -A lets codesign use the key without a keychain-access prompt on every build.
security import "$TMP/id.p12" -k "$KEYCHAIN" -P "$PW" -T /usr/bin/codesign -A

echo
echo "Done. Installed identity:"
security find-identity -p codesigning | grep "$ID" || true
echo
echo "Next:"
echo "  1. Rebuild the app:  cmake --build build --target SculptSampler_Standalone"
echo "  2. Launch it and grant folder access once when macOS asks."
echo "     From then on the grant persists across rebuilds."
