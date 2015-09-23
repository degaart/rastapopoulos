export PREFIX="/Volumes/Kratos/Projects/RastapopoulOS"

env -i \
    PREFIX="$PREFIX" \
    PATH="$PREFIX/bin:$PREFIX/sbin:/bin:/usr/bin" \
    TERM="$TERM" \
    TMPDIR="$TMPDIR" \
    LC_ALL="$LC_ALL" \
    USER="$USER" \
    EDITOR="$EDITOR" \
    SSH_AUTH_SOCK="$SSH_AUTH_SOCK" \
    __CF_USER_TEXT_ENCODING="$__CF_USER_TEXT_ENCODING" \
    TMUX="$TMUX" \
    LANG="$LANG" \
    PS1="$PS1" PS2="$PS2" \
    DISPLAY="$DISPLAY" \
    HOME="$HOME" \
    /bin/bash



