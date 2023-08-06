for dir in bootloader kernel user tools
do
    make -C "$dir" format
done

make -C common -f common.mk format

