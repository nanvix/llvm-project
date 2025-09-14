#!/usr/bin/env bash
# Unified helper to fetch latest nanvix/gcc release information and update
# GCC related readonly variables inside the repository's `z` file if needed.

#
# Usage (all arguments required):
#   .nanvix/update.sh <api_url> <asset_regex> <parse_regex> <z-file> <var_prefix>
# Example:
#   .nanvix/update.sh \\
#     https://api.github.com/repos/nanvix/gcc/releases/latest \\
#     'nanvix-gcc-.*-stage1\.tar\.bz2$' \\
#     'nanvix-gcc-([0-9]+\.[0-9]+\.[0-9]+)-([0-9a-fA-F]+)-stage1\.tar\.bz2' \\
#     GCC
#
# Outputs (GitHub Actions style, written to $GITHUB_OUTPUT or stdout fallback):
#   version=<ver>                Parsed GCC version (when asset found)
#   short_hash=<hash>            Parsed short commit hash (when asset found)
#   asset_name=<name>            Selected asset file name
#   asset_digest=<digest>        Asset digest if present in release metadata (may be empty)
#   current_version=<ver>        Current version found in z (may be empty)
#   current_short_hash=<hash>    Current short hash found in z (may be empty)
#   changed=true|false           Whether z file was modified
#   updated_vars=comma,list      Variables actually updated when changed=true
#
# Exit codes:
#   0 Success (even if no update needed or no asset)
#   1 Argument / environment error
#   2 Update logic error

set -euo pipefail

SCRIPT_DIR=
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || {
  echo "::error ::Failed to determine script directory" >&2
  exit 1
}

readonly Z_FILE="${SCRIPT_DIR}/../z"

if [ "$#" -ne 4 ]; then
  echo "::error ::Invalid number of arguments ($# provided)." >&2
  echo "Usage: $0 <api_url> <asset_regex> <parse_regex><var_prefix>" >&2
  exit 1
fi

api=$1
asset_regex=$2
parse_regex=$3
var_prefix=$4

if [ -z "$api" ]; then
  echo "::error ::API URL argument is required" >&2
  exit 1
fi
if [ -z "$asset_regex" ]; then
  echo "::error ::Asset regex argument is required" >&2
  exit 1
fi
if [ -z "$parse_regex" ]; then
  echo "::error ::Parse regex argument is required" >&2
  exit 1
fi
if [ -z "$var_prefix" ]; then
  echo "::error ::Variable prefix argument is required (or default failed)" >&2
  exit 1
fi
# Validate prefix as a shell variable name (basic check)
if ! [[ $var_prefix =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
  echo "::error ::Variable prefix '$var_prefix' is not a valid shell identifier" >&2
  exit 1
fi

version_var="${var_prefix}_VERSION"
short_hash_var="${var_prefix}_SHORT_COMMIT_HASH"
checksum_var="${var_prefix}_SHA256_CHECKSUM"

if ! command -v jq >/dev/null 2>&1; then
  echo "::error ::jq is not installed and is required" >&2
  exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
  echo "::error ::curl is not installed and is required" >&2
  exit 1
fi

out=${GITHUB_OUTPUT:-/dev/stdout}

echo "Fetching release metadata: $api" >&2
resp=$(curl -sL "$api" || true)
if [ -z "$resp" ]; then
  echo "::error ::Empty response from API" >&2
  exit 1
fi

# Attempt to select asset.
asset=$(echo "$resp" | jq -r --arg rx "$asset_regex" '.assets[] | select(.name | test($rx))' || true)
if [ -z "$asset" ]; then
  echo "No matching asset found (regex: $asset_regex)" >&2
  {
    echo "no_asset=true"
    echo "changed=false"
    echo "updated_vars="
    echo "version="
    echo "short_hash="
    echo "asset_name="
    echo "asset_digest="
    echo "current_version="
    echo "current_short_hash="
  } >> "$out"
  exit 0
fi

echo "Asset candidate found" >&2
asset_name=$(echo "$asset" | jq -r '.name')
asset_digest=$(echo "$asset" | jq -r '.digest // empty')

version=""
short_hash=""
if [[ "$asset_name" =~ $parse_regex ]]; then
  version=${BASH_REMATCH[1]}
  short_hash=${BASH_REMATCH[2]}
else
  echo "::error ::Failed to parse version/hash from asset name using regex: $parse_regex" >&2
  exit 2
fi

echo "Latest release parsed: version=$version short_hash=$short_hash" >&2

# Extract current values from z file if present.
current_version=""
current_short_hash=""
if [ -f "$Z_FILE" ]; then
  current_version=$(sed -nE "s/^readonly[[:space:]]+${version_var}=\"?([^\" ]+)\"?/\\1/p" "$Z_FILE" | head -n1 || true)
  current_short_hash=$(sed -nE "s/^readonly[[:space:]]+${short_hash_var}=\"?([^\" ]+)\"?/\\1/p" "$Z_FILE" | head -n1 || true)
else
  echo "::warning file=${Z_FILE}::z file not found; treating as empty values" >&2
fi

echo "Current z values: version=${current_version:-<none>} short_hash=${current_short_hash:-<none>}" >&2

# Decide if update needed.
if [ "$version" = "$current_version" ] && [ "$short_hash" = "$current_short_hash" ]; then
  echo "Already up to date; no changes required" >&2
  {
    echo "no_asset=false"
    echo "version=$version"
    echo "short_hash=$short_hash"
    echo "asset_name=$asset_name"
    echo "asset_digest=$asset_digest"
    echo "current_version=$current_version"
    echo "current_short_hash=$current_short_hash"
    echo "changed=false"
    echo "updated_vars="
  } >> "$out"
  exit 0
fi

echo "Update required; modifying $Z_FILE" >&2

if [ ! -f "$Z_FILE" ]; then
  echo "::error file=${Z_FILE}::z file not found in repository root" >&2
  exit 2
fi

# We'll update three variables: <prefix>_SHORT_COMMIT_HASH, <prefix>_SHA256_CHECKSUM, <prefix>_VERSION.
updated=()
changed=false

update_var() {
  local var=$1 val=$2 esc_val line current_val
  esc_val=$(printf '%s' "$val" | sed 's/[\/&]/\\&/g')

  # Find first matching definition (if any).
  line=$(grep -m1 -E "^(readonly[[:space:]]+)?${var}=" "$Z_FILE" || true)

  if [ -n "$line" ]; then
    # Extract RHS, strip comments/whitespace and optional surrounding quotes.
    current_val=$(printf '%s' "$line" | sed -E "s/^[^=]+=//; s/[[:space:]]*#.*$//; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^\"(.*)\"$/\1/; s/^'(.*)'$/\1/")
  else
    current_val=""
  fi

  if [ "$current_val" = "$val" ]; then
    echo "No change for $var (already $val)" >&2
    return 0
  fi

  if [ -n "$line" ]; then
    sed -E -i "s|^(readonly[[:space:]]+)?${var}=.*$|readonly ${var}=\"${esc_val}\"|" "$Z_FILE"
  else
    echo "readonly ${var}=\"${val}\"" >> "$Z_FILE"
  fi

  updated+=("$var")
  changed=true
  echo "Updated $var -> $val" >&2
}

update_var "$short_hash_var" "$short_hash"
update_var "$checksum_var" "$asset_digest"
update_var "$version_var" "$version"

{
  echo "no_asset=false"
  echo "version=$version"
  echo "short_hash=$short_hash"
  echo "asset_name=$asset_name"
  echo "asset_digest=$asset_digest"
  echo "current_version=$current_version"
  echo "current_short_hash=$current_short_hash"
  echo "changed=$changed"
  echo "updated_vars=$(IFS=,; echo "${updated[*]}")"
} >> "$out"

echo "Done. changed=$changed updated_vars=${updated[*]:-}" >&2
exit 0
