#!/bin/bash
set -eou pipefail

pushd "${BASH_SOURCE[0]%/*}" > /dev/null
SCRIPT_PATH="$PWD"
popd > /dev/null

pushd "$SCRIPT_PATH/../.." > /dev/null
PREFIX="$PWD"
popd > /dev/null

TARGET=i686-elf

export PATH="$PREFIX/bin:$PREFIX/sbin:$PATH"

while IFS= read -r package; do
    if [ -n "$package" ]; then
        package_file="$SCRIPT_PATH/packages/${package}.pkg"

        if ! [ -f "$PREFIX/var/packages/$package" ]; then
            unset VERSION SOURCE_URL GIT_URL GIT_COMMIT
            source "$package_file"

            if [ -n "${SOURCE_URL:-}" ]; then
                tarball="$(basename "$SOURCE_URL")"

                if ! [ -f "$PREFIX/tarballs/$tarball" ]; then
                    echo "*** Downloading $package ***"
                    mkdir -p "$PREFIX/tarballs"
                    cd "$PREFIX/tarballs"
                    wget -c "$SOURCE_URL"
                fi

                echo "*** Extracting $package ***"
                rm -rf "$PREFIX/build/${package}-src"
                mkdir -p "$PREFIX/build/${package}-src"
                cd "$PREFIX/build/${package}-src"

                case "${tarball##*.}" in
                    xz)
                        pv "$PREFIX/tarballs/$tarball"|tar Jxf -
                        ;;
                    zst)
                        pv "$PREFIX/tarballs/$tarball"|tar xf - -I zstd
                        ;;
                    bz2)
                        pv "$PREFIX/tarballs/$tarball"|tar jxf -
                        ;;
                    gz)
                        pv "$PREFIX/tarballs/$tarball"|tar zxf -
                        ;;
                    tar)
                        pv "$PREFIX/tarballs/$tarball"|tar xf -
                        ;;
                    *)
                        tar xf "$PREFIX/tarballs/$tarball"
                        ;;
                esac
            elif [ -n "${GIT_URL:-}" ]; then
                echo "*** Cloning $package ***"
                rm -rf "$PREFIX/build/${package}-src"
                mkdir -p "$PREFIX/build/${package}-src"
                cd "$PREFIX/build/${package}-src"
                git clone --depth=1 "$GIT_URL"
            else
                echo "Invalid package: $package_file" >&2
                exit 1
            fi

            echo "*** Building $package ***"
            rm -rf "$PREFIX/build/${package}-build"
            mkdir -p "$PREFIX/build/${package}-build"
            cd "$PREFIX/build/${package}-build"
            if ! build "$PREFIX/build/${package}-src" > "$PREFIX/build/${package}.log" 2>&1; then
                echo "Build of $package failed" >&2
                echo "Logfile: $PREFIX/build/${package}.log" >&2
                exit 1
            fi
            mkdir -p "$PREFIX/var/packages"
            echo "${VERSION:-$GIT_COMMIT}" > "$PREFIX/var/packages/$package"
            rm -rf "$PREFIX/build/$package"-{src,build}
        fi
    fi
done < "$SCRIPT_PATH/packages/packages.idx"

echo "export PREFIX='$PREFIX'" > "$PREFIX/environment.sh"
echo 'export PATH="$PREFIX/bin:$PREFIX/sbin:$PATH"' >> "$PREFIX/environment.sh"
[ -n "${HOSTDATA:-}" ] && echo "export HOSTDATA=\"$HOSTDATA\"" >> "$PREFIX/environment.sh"

