main() {
    cleanvars
    set_prefix

    path_prepend "$PREFIX/bin"
    is_osx && {
        echo "WARNING: removing /usr/local/bin from path"
        path_remove "/usr/local/bin"

        set_osx_target "10.5"
        set_osx_arch "i386"
        set_osx_sdk "10.6"

        export CC=gcc-4.2
        export CPP=cpp-4.2
        export CXX=g++-4.2
        export LD=g++-4.2
	#unset CC CPP CXX LD

        export DYLD_LIBRARY_PATH="$PREFIX/lib"
    }

    is_linux && {
        export LD_LIBRARY_PATH="$PREFIX/lib"
    }

    export CPPFLAGS="$archflags -I$PREFIX/include"
    export CFLAGS="$archflags"
    export CXXFLAGS="$archflags"
    export LDFLAGS="$archflags -L$PREFIX/lib -L/opt/X11/lib"
    export PKG_CONFIG_PATH="$PREFIX/share/pkgconfig"

    #export JDK_HOME=/System/Library/Java/JavaVirtualMachines/1.6.0.jdk/Contents/Home/

    cleanfuncs
}

cleanvars() {
    unset CPP CC CXX LD CPPFLAGS CFLAGS CXXFLAGS LDFLAGS
    unset LD_LIBRARY_PATH
    unset PKG_CONFIG_PATH
    unset archflags
}

set_prefix() {
    # Set prefix to script path
    SCRIPT_PATH="${BASH_SOURCE[0]}";
    while([ -h "${SCRIPT_PATH}" ])
    do
        SCRIPT_PATH=$(readlink "${SCRIPT_PATH}")
    done
    pushd . > /dev/null
    cd $(dirname "${SCRIPT_PATH}") > /dev/null
    SCRIPT_PATH=$(pwd)
    popd  > /dev/null
    export PREFIX="$SCRIPT_PATH/.."
}

path_append ()
{
    path_remove $1
    export PATH="$PATH:$1"
}

path_prepend ()
{
    path_remove $1
    export PATH="$1:$PATH"
}

path_remove ()
{
    export PATH=`echo -n $PATH | awk -v RS=: -v ORS=: '$0 != "'$1'"' | sed 's/:$//'`
}

is_osx() {
    [ "$(uname)" = 'Darwin' ]
}

is_linux() {
    [ "$(uname)" = 'linux' ]
}

set_osx_sdk() {
    local xpath="$(xcode-select -print-path)"
    local sdkpath

    if [ -d "${xpath}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${1}.sdk" ]; then
        sdkpath="${xpath}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${1}.sdk"
    elif [ -d "${xpath}/SDKs/MacOSX${1}.sdk" ]; then
        sdkpath="${xpath}/SDKs/MacOSX${1}.sdk"
    fi

    if [ -z "$sdkpath" ]; then
        echo "No SDK found for $1"
        exit 1
    fi

    archflags="$archflags -isysroot $sdkpath"
}

set_osx_arch() {
    archflags="$archflags -arch $1"
}

set_osx_target() {
    archflags="$archflags -mmacosx-version-min=$1"
}

cleanfuncs() {
    unset archflags
    unset cleanvars set_prefix path_append path_prepend path_remove
    unset is_osx is_linux
    unset set_osx_sdk set_osx_arch set_osx_target
    unset cleanfuncs
    unset main
}

main "$@"
