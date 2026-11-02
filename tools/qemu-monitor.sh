echo "$@"|socat - UNIX-CONNECT:/tmp/qemu-monitor.sock

