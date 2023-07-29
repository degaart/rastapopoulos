for dir in bootloader kernel user tools
do
 #-exec clang-format -i '{}' ';'
    find "$dir" -name '*.c' -or -name '*.h'
done

