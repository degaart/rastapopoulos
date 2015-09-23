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

export PREFIX=/opt/llvm-3.7
path_remove "/usr/local/bin"
path_remove "/usr/local/sbin"
path_prepend "$PREFIX/bin"


