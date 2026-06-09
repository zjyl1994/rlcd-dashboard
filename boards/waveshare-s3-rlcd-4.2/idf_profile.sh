#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:-}"

if [[ -z "$MODE" ]]; then
    printf 'Usage: %s {dev|prod} [idf.py args...]\n' "$0" >&2
    exit 1
fi

shift

case "$MODE" in
    dev)
        BUILD_DIR="$SCRIPT_DIR/build-dev"
        SDKCONFIG_FILE="$SCRIPT_DIR/sdkconfig.dev"
        SDKCONFIG_DEFAULTS="$SCRIPT_DIR/sdkconfig.defaults;$SCRIPT_DIR/sdkconfig.defaults.dev"
        ;;
    prod)
        BUILD_DIR="$SCRIPT_DIR/build-prod"
        SDKCONFIG_FILE="$SCRIPT_DIR/sdkconfig.prod"
        SDKCONFIG_DEFAULTS="$SCRIPT_DIR/sdkconfig.defaults;$SCRIPT_DIR/sdkconfig.defaults.prod"
        ;;
    *)
        printf 'Usage: %s {dev|prod} [idf.py args...]\n' "$0" >&2
        exit 1
        ;;
esac

if [[ $# -eq 0 ]]; then
    set -- build
fi

if [[ -f "$SCRIPT_DIR/../../../esp-idf/export.sh" ]]; then
    # shellcheck disable=SC1091
    . "$SCRIPT_DIR/../../../esp-idf/export.sh" >/dev/null
fi

idf.py -B "$BUILD_DIR" -D SDKCONFIG="$SDKCONFIG_FILE" -D SDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" "$@"
