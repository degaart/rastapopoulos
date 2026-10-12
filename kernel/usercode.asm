section .rodata
align 16

global usercode_start
global usercode_end

usercode_start:
    incbin "../user/build/user.bin"
usercode_end:

