absolute_path() {
    local filename
    local base
    local path

    filename="$1"
    base="$(basename "$1")"
    path="$(dirname "$1")"

    [ -n "$filename" ] || exit 1

    pushd . > /dev/null
    cd "$path" || exit 1
    echo "$PWD/$base"
    popd > /dev/null || exit 1
}

cleanup() {
    [ -n "$MTOOLS_TMP" ] && rm -f "$MTOOLS_TMP"
}

mtools_init() {
    [ -n "$1" ] || {
        echo "Usage: mtools_init <image_file>"
        exit 1
    }

    local filename="$(absolute_path "$1")"

    # Create configuration file
    MTOOLS_TMP="$(mktemp -t "$PROGRAM_NAME")"
    export MTOOLSRC="$MTOOLS_TMP"

    cat << EOF > "$MTOOLSRC"
drive L:
    file="$filename"
    partition=1
    mformat_only
EOF
}

trap cleanup EXIT
PROGRAM_NAME="$(basename "$0")"
set -o pipefail

