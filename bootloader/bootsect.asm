; FAT12 boot sector for a standard 1.44 MiB floppy.
; Loads RASTALDR.BIN at 1000:0000 (physical 0x10000) and jumps to it.
bits    16
cpu     8086
org     0x7c00

    ; constants
    BPB_BYTES_PER_SECTOR    equ 512
    BPB_RESERVED_SECTORS    equ 1
    BPB_ROOT_ENTRIES        equ 224
    BPB_FAT_COUNT           equ 2
    BPB_SECTORS_PER_FAT     equ 9
    FAT12_FREE              equ 0xe5
    FIRST_DATA_LBA          equ 33
    LOAD_SEGMENT            equ 0x0800

    ; work buffers
    FAT_BUFFER              equ 0x500
    DISK_LBA                equ FAT_BUFFER + (512 * 9)
    CLUSTER                 equ DISK_LBA + 2
    REMAINING               equ CLUSTER + 2
    PANIC_CODE              equ REMAINING + 4
    ROOT_BUFFER             equ PANIC_CODE + 1

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

    ; debugging macro
%macro print_char 1
    mov al, %1
    mov ah, 0x0e
    mov bx, 0x0007
    int 0x10
%endmacro

%macro isabugger0 1
    mov  al, 0x02
    out  0x7a, al
    mov  al, %1
    out  0x7b, al
%endmacro

%macro isabugger1 1
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

    ; skip BPB
    jmp  short boot
    nop

    ; BIOS Parameter Block (BPB)
bpb:
    .oem_name:            db 'RASTAOS '
    .bytes_per_sector:    dw BPB_BYTES_PER_SECTOR
    .sectors_per_cluster: db 1
    .reserved_sectors:    dw BPB_RESERVED_SECTORS
    .fat_count:           db BPB_FAT_COUNT
    .root_entries:        dw BPB_ROOT_ENTRIES
    .total_sectors_16:    dw 2880
    .media_descriptor:    db 0xf0
    .sectors_per_fat:     dw BPB_SECTORS_PER_FAT
    .sectors_per_track:   dw 18
    .head_count:          dw 2
    .hidden_sectors:      dd 0
    .total_sectors_32:    dd 0
    .boot_drive:          db 0
    .reserved:            db 0
    .extended_signature:  db 0x29
    .volume_serial:       dd 0x52535441
    .volume_label:        db 'RASTA FLOPPY'
    .filesystem_type:     db 'FAT12   '
    .end:

boot:
    ; setup
    cli
    mov  ax, cs
    mov  ds, ax
    xor  dx, dx
    mov  ss, dx
    mov  sp, 0x7c00
    cld
    mov  [bpb.boot_drive], dl

    ; Read first FAT
    mov  ax, 1
    mov  cx, [bpb.sectors_per_fat]
    mov  bx, FAT_BUFFER
    call read_sectors

    mov  si, bpb.bytes_per_sector

.loop:
    mov  bx, si
    cmp  bx, bpb.end
    jae  halt

    isabugger1 bl
    isabugger0 [si]

    xor  ah, ah
    int  0x16

    inc  si
    jmp  .loop

    ; read root directory
    ; lba = reserved_sectors + (fat_count * sectors_per_fat)
    ; size_in_sectors = (root_entries * 32) / bytes_per_sector
    mov  ax, [bpb.fat_count]
    mul  word [bpb.sectors_per_fat]
    add  ax, [bpb.reserved_sectors]
    mov  bx, ROOT_BUFFER
    call read_sectors

    mov  [PANIC_CODE], byte '*'
    jmp  panic

    ; find loader
    mov  di, ROOT_BUFFER
find_ldr:
    mov  [PANIC_CODE], byte '2'
    cmp  byte [di], 0
    je   panic
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
    mov  [CLUSTER], ax
    mov  ax, [di + Dirent.file_size]
    mov  dx, [di + Dirent.file_size + 2]
    mov  [REMAINING], ax
    mov  [REMAINING + 2], dx
    mov  [PANIC_CODE], byte '3'
    or   ax, dx                     ; check bot AX and DX are zero
    jz   panic

    mov  ax, LOAD_SEGMENT
    mov  es, ax

load_cluster:
    mov  ax, [CLUSTER]
    mov  [PANIC_CODE], byte '4'
    cmp  ax, 2
    jb   panic

    mov  [PANIC_CODE], byte '5'
    cmp  ax, 0x0ff0
    jae  panic

    ; general formula is lba = FIRST_DATA_LBA + (cluster - 2) * sectors_per_cluster
    ; but sectors_per_cluster is always 1
    ; so we can simplify to lba = cluster + FIRST_DATA_LBA - 2
    add  ax, FIRST_DATA_LBA - 2
    xor  bx, bx
    call read_sector
    cmp  word [REMAINING + 2], 0
    jne  .next
    cmp  word [REMAINING], 512
    jbe  launch
.next:
    sub  word [REMAINING], 512
    sbb  word [REMAINING + 2], 0

    ; next fat12 entry
    mov  ax, [CLUSTER]
    mov  bx, ax
    shr  bx, 1
    add  bx, ax
    mov  dx, [bx + FAT_BUFFER]
    test ax, 1
    jz   .even
    mov  cl, 4
    shr  dx, cl
.even:
    and  dx, 0x0fff
    mov  [CLUSTER], dx
    mov  [PANIC_CODE], byte '6'
    cmp  dx, 0x0ff8
    jae  panic

    mov  ax, es
    add  ax, 0x20                   ; 512 bytes
    mov  es, ax
    jmp  load_cluster

launch:
    jmp  LOAD_SEGMENT:0

    ; ===================================================
    ;mov  si, message
    ;call puts
    jmp  halt


; Read CX sectors beginning at LBA AX into es:BX.
; Preserves all caller-visible registers.
read_sectors:
    call read_sector
    inc ax
    add bx, 512
    loop read_sectors
    ret

; Read one LBA sector from the boot drive into ES:BX.
; Preserves all caller-visible registers.
read_sector:
    push cx
    push dx
    push bp
    push si
    push di

    mov  [DISK_LBA], ax
    mov  si, 3
.retry:
    mov  ax, [DISK_LBA]
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

    mov  [PANIC_CODE], byte '7'
    jmp  panic
.ok:
    pop  di
    pop  si
    pop  bp
    pop  dx
    pop  cx
    ret

; print string
; args:
;   si      string address
; trashes: ax, bx, si
puts:
    push bp
.loop:
    mov  ah, 0x0e
    mov  bx, 0x0007
    mov  al, [si]
    cmp  al, 0
    je   .return
    int  0x10
    inc  si
    jmp  .loop
.return:
    pop  bp
    ret

; panic with the error code in [PANIC_CODE]
panic:
    mov  si, panic_message
    call puts
    mov  al, [PANIC_CODE]
    mov  ah, 0x0e
    mov  bx, 0x0007
    int  0x10

halt:
    cli
    hlt
    jmp  halt

message: db `KAKA\r\n`, 0
panic_message: db `PANIC `, 0
loader_name: db "RASTALDRBIN"

times 510 - ($ - $$) db 0
dw 0xaa55

