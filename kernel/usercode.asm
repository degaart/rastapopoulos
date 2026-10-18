section .rodata
align 16

struc UserCode
    .name   resd 1
    .len    resd 1
    .addr   resd 1
endstruc

snek_name: db `snek`, 0
snek_start:
    incbin "../user/snek/build/snek.bin"
snek_end:

global usercode_data
usercode_data:
istruc UserCode
    at .name,       dd snek_name
    at .len,        dd snek_end - snek_start
    at .addr,       dd snek_start
iend

istruc UserCode
    at .name,       dd 0
    at .len,        dd 0
    at .addr,       dd 0
iend
