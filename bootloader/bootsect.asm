; FAT12 boot sector for a standard 1.44 MiB floppy.
; Loads RASTALDR.BIN at 1000:0000 (physical 0x10000) and jumps to it.
bits    16
cpu     8086
org     0x7c00

    ; constants
    FAT12_FREE              equ 0xe5
    FIRST_DATA_LBA          equ 7
    LOAD_SEGMENT            equ 0x0800

    ; Work buffers
    VARS                    equ 0x500
struc V
    .disk_lba               resw 1
    .cluster                resw 1
    .remaining              resd 1
    .panic_code             resb 1
    .root_dir_lba           resw 1
    .root_dir_size_sect     resw 1
    .first_data_lba         resw 1
    .fat_buffer             resb 512 * 9                    ; intentionally limit to 9 sectors
    .root_buffer            resb 1                          ; variable-length
endstruc

    ; types
struc Dirent
    .filename:          resb    8       ; Filename (padded with spaces)
    .extension:         resb    3       ; Extension (padded with spaces)
    .attributes:        resb    1       ; File attributes (hidden, system, dir, etc.)
    .reserved_nt:       resb    1       ; Reserved for NT (lowercase flags)
    .creation_time_ms:  resb    1       ; Creation time tenths of a second
    .creation_time:     resw    1       ; Creation time (Hour:5, Min:6, Sec:5/2)
    .creation_date:     resw    1       ; Creation date (Year:7, Month:4, Day:5)
    .last_access_date:  resw    1       ; Last access date (Year:7, Month:4, Day:5)
    .first_cluster_hi:  resw    1       ; High 16-bits of cluster (0 in FAT12)
    .write_time:        resw    1       ; Last modification time
    .write_date:        resw    1       ; Last modification date
    .first_cluster_low: resw    1       ; Low 16-bits of cluster (Starting cluster)
    .file_size:         resd    1       ; File size in bytes
endstruc

    ; debugging macros
%macro print_char 1
    mov al, %1
    mov ah, 0x0e
    mov bx, 0x0007
    int 0x10
%endmacro

%macro isabuggerR 1
    mov  al, 0x02
    out  0x7a, al
    mov  al, %1
    out  0x7b, al
%endmacro

%macro isabuggerL 1
    mov  al, 0x04
    out  0x7a, al
    mov  al, %1
    out  0x7b, al
%endmacro

%macro debugcon 0-1
    %if %0 = 1
        mov  al, %1
    %endif
    out  0xe9, al
%endmacro

%macro panic 1
    mov  byte [VARS+V.panic_code], %1
    jmp  _panic
%endmacro

%macro assert 2
    %1 %%end
    panic %2
%%end:
%endmacro

    ; skip BPB
    jmp  short boot
    nop

    ; BIOS Parameter Block (BPB)
bpb:
    .oem_name:            db 'RASTAOS '
    .bytes_per_sector:    dw 0
    .sectors_per_cluster: db 0
    .reserved_sectors:    dw 0
    .fat_count:           db 0
    .root_entries:        dw 0
    .total_sectors_16:    dw 0
    .media_descriptor:    db 0
    .sectors_per_fat:     dw 0
    .sectors_per_track:   dw 0
    .head_count:          dw 0
    .hidden_sectors:      dd 0
    .total_sectors_32:    dd 0
    .boot_drive:          db 0
    .reserved:            db 0
    .extended_signature:  db 0
    .volume_serial:       dd 0
    .volume_label:        db '            '
    .filesystem_type:     db '        '
    .end:

boot:
    ; setup
    cli
    jmp 0:.setupsegs

.setupsegs:
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7c00
    cld
    mov  [bpb.boot_drive], dl

    ; Read first FAT
    mov  ax, 1
    mov  cx, [bpb.sectors_per_fat]
    cmp  cx, 9
    jbe  .readfat
    panic '0'
.readfat:
    mov  bx, VARS+V.fat_buffer
    call read_sectors

    ; read root directory
    ; lba = reserved_sectors + (fat_count * sectors_per_fat)
    ; size_in_sectors = (root_entries * 32) / bytes_per_sector
    mov  ax, [bpb.root_entries]
    mov  cl, 5
    shl  ax, cl                             ; ax *= 32
    xor  dx, dx
    div  word [bpb.bytes_per_sector]        ; ax /= bpb.bytes_per_sector
    mov  cx, ax                             ; cx = size in sectors
    mov  [VARS+V.root_dir_size_sect], cx    ; we need it when calculating first data cluster lba

    xor  ax, ax
    mov  al, [bpb.fat_count]
    mul  word [bpb.sectors_per_fat]
    add  ax, [bpb.reserved_sectors]     ; ax = lba
    mov  [VARS+V.root_dir_lba], ax

    mov  bx, VARS+V.root_buffer
    call read_sectors

     ; find loader
     mov  di, VARS+V.root_buffer
find_ldr:
     cmp  byte [di], 0
     jne  .check_free
     panic '1'

.check_free:
     cmp  byte [di], FAT12_FREE
     je   .next

     ; compare with loader name
     push di
     mov  si, loader_name
     mov  cx, 11
     repe cmpsb
     pop  di
     je   .found
 .next:
     add  di, Dirent_size
     dec  dx
     jnz  find_ldr
 
 .found:
    mov  ax, [di + Dirent.first_cluster_low]
    mov  [VARS+V.cluster], ax
    mov  ax, [di + Dirent.file_size]
    mov  dx, [di + Dirent.file_size + 2]
    mov  [VARS+V.remaining], ax
    mov  [VARS+V.remaining+2], dx
    or   ax, dx                     ; check both AX and DX are zero
    jnz  .check_size
    panic '2'

.check_size:
    int  0x12

    ; shift left ax by 10, putting the high word in DX
    ; ax = low 16 bits of (original << 10)
    ; dx = high 16 bits = original >> 6
    mov  dx, ax
    mov  cl, 10
    shl  ax, cl
    mov  cl, 6
    shr  dx, cl

    ; compare to (dx:ax) - (LOAD_SEGMENT / 16)
    sub  ax, LOAD_SEGMENT / 16
    sbb  dx, 0
    cmp  dx, [VARS+V.remaining+2]
    ja   .load_es
    cmp  ax, [VARS+V.remaining]
    ja   .load_es
    panic '3'

.load_es:
    mov  ax, LOAD_SEGMENT
    mov  es, ax

    ; save LBA of first data LBA
    mov  ax, [VARS+V.root_dir_lba]
    add  ax, [VARS+V.root_dir_size_sect]
    mov  [VARS+V.first_data_lba], ax

load_cluster:
    mov  ax, [VARS+V.cluster]
    cmp  ax, 2
    jae  .check_clus
    panic '4'

.check_clus:
    cmp  ax, 0x0ff0
    jb   .read_clus
    panic '5'

.read_clus:
    ; general formula is lba = FIRST_DATA_LBA + (cluster - 2) * sectors_per_cluster
    sub  ax, 2
    xor  bx, bx
    mov  bl, [bpb.sectors_per_cluster]
    mul  bx
    add  ax, [VARS+V.first_data_lba]

    xor  bx, bx
    call read_sector

    cmp  word [VARS+V.remaining+2], 0
    jne  .next
    cmp  word [VARS+V.remaining], 512
    jbe  launch
.next:
    sub  word [VARS+V.remaining], 512
    sbb  word [VARS+V.remaining+2], 0

    ; next fat12 entry
    mov  ax, [VARS+V.cluster]
    mov  bx, ax
    shr  bx, 1
    add  bx, ax
    mov  dx, [bx + VARS+V.fat_buffer]
    test ax, 1
    jz   .even
    mov  cl, 4
    shr  dx, cl
.even:
    and  dx, 0x0fff
    mov  [VARS+V.cluster], dx
    cmp  dx, 0x0ff8
    jb   .inc_es
    panic '6'

.inc_es:
    mov  ax, es
    add  ax, 0x20                   ; 512 bytes
    mov  es, ax
    jmp  load_cluster

launch:
    mov  ax, LOAD_SEGMENT
    mov  es, ax
    jmp  LOAD_SEGMENT:0

; Read CX sectors beginning at LBA AX into es:BX.
; Trashes ax and bx
read_sectors:
    call read_sector
    inc ax
    add bx, 512
    loop read_sectors
    ret

; Read one AX=LBA sector from the boot drive into ES:BX.
; Preserves all caller-visible registers.
read_sector:
    push ax
    push bx
    push cx
    push dx
    push bp
    push si
    ;push di

    mov  [VARS+V.disk_lba], ax
    mov  si, 3
.retry:
    mov  ax, [VARS+V.disk_lba]
    xor  dx, dx
    div  word [bpb.sectors_per_track]
    inc  dl
    mov  cl, dl                    ; sector (1..18)
    xor  dx, dx
    div  word [bpb.head_count]
    mov  ch, al                    ; cylinder (0..79)
    mov  dh, dl                    ; head (0..1)
    mov  dl, [bpb.boot_drive]
    mov  ax, 0x0201
    int  0x13
    jnc  .ok
    xor  ax, ax
    int  0x13
    dec  si
    jnz  .retry                    ; recompute CHS after the BIOS reset
    panic '7'
.ok:
    ;pop  di
    pop  si
    pop  bp
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; panic with the error code in [PANIC_CODE]
; use the panic macro
_panic:
    mov  al, [VARS+V.panic_code]
    mov  ah, 0x0e
    mov  bx, 0x0007
    int  0x10

halt:
    jmp  halt

loader_name: db "RASTALDRBIN"

times 510 - ($ - $$) db 0
dw 0xaa55

