for dir in bootloader kernel user tools
do
    find "$dir" -name '*.c' -or -name '*.h' -exec clang-format -i '{}' ';'
done

