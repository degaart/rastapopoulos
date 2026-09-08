; FAT12 boot sector for a standard 1.44 MiB floppy.
; Loads RASTALDR.BIN at 1000:0000 (physical 0x10000) and jumps to it.
bits 16
org  0x7c00

    ; constants
    BPB_BYTES_PER_SECTOR    equ 512
    BPB_RESERVED_SECTORS    equ 1
    BPB_ROOT_ENTRIES        equ 224
    BPB_FAT_COUNT           equ 2
    BPB_SECTORS_PER_FAT     equ 9
    FAT12_FREE              equ 0xE5
    FIRST_DATA_LBA          equ 33

    ; work buffers
    FAT_BUFFER              equ 0x500
    DISK_LBA                equ FAT_BUFFER + (512 * 9)
    CLUSTER                 equ DISK_LBA + 2
    REMAINING               equ CLUSTER + 2
    ROOT_BUFFER             equ REMAINING + 4

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
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7c00
    cld
    mov  [bpb.boot_drive], dl

    ; Read first FAT
    mov  ax, 1
    mov  cx, BPB_SECTORS_PER_FAT
    mov  bx, FAT_BUFFER
    call read_sectors

    ; Read the complete root directory
    ; lba = reserved_sectors + (fat_count * sectors_per_fat)
    ; size_in_sectors = (root_entries * 32) / bytes_per_sector
    mov  ax, BPB_RESERVED_SECTORS + (BPB_FAT_COUNT * BPB_SECTORS_PER_FAT)
    mov  cx, (BPB_ROOT_ENTRIES * 32) / BPB_BYTES_PER_SECTOR
    mov  bx, ROOT_BUFFER
    call read_sectors

    ; find the loader inside root entries
    mov  di, ROOT_BUFFER
find_ldr:
    cmp  byte [di], 0
    je   .not_found
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
.not_found:
    jmp  halt

.found:
    mov  ax, [di + Dirent.first_cluster_low]
    mov  [CLUSTER], ax
    mov  ax, [di + Dirent.file_size]
    mov  dx, [di + Dirent.file_size + 2]
    mov  [REMAINING],   ax
    mov  [REMAINING + 2], dx
    or   ax, dx
    jz   fatal                  ; check whether both are zero

    mov  ax, 0x1000
    mov  es, ax

load_cluster:
    mov  ax, [CLUSTER]
    cmp  ax, 2
    jb   fatal
    cmp  ax, 0x0FF0
    jae  fatal

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
    shr  dx, 4
.even:
    and  dx, 0x0FFF
    mov  [CLUSTER], dx
    cmp  dx, 0x0FF8
    jae  fatal

    mov  ax, es
    add  ax, 0x20                   ; 512 bytes
    mov  es, ax
    jmp  load_cluster

launch:
    jmp 0x1000:0

halt:
    hlt
    jmp halt

; Read CX sectors beginning at LBA AX into es:BX.
read_sectors:
    call read_sector
    inc ax
    add bx, 512
    loop read_sectors
    ret

; Read one LBA sector from the boot drive into ES:BX.
; Preserves all caller-visible registers.
read_sector:
    mov [DISK_LBA], ax
    pusha
    mov si, 3
.retry:
    mov ax, [DISK_LBA]
    xor dx, dx
    div word [bpb.sectors_per_track]
    inc dl
    mov cl, dl                    ; sector (1..18)
    xor dx, dx
    div word [bpb.head_count]
    mov ch, al                    ; cylinder (0..79)
    mov dh, dl                    ; head (0..1)
    mov dl, [bpb.boot_drive]
    mov ax, 0x0201
    int 0x13
    jnc .ok
    xor ax, ax
    int 0x13
    dec si
    jnz .retry                    ; recompute CHS after the BIOS reset
    jmp fatal
.ok:
    popa
    ret

; in:       si = asciiz string to print
; clobbers: ax, bx, si
puts:
    mov ah, 0x0E
    mov bx, 0x0007

.loop:
    mov al, [si]
    test al, al
    jz .return
    int 0x10
    inc si
    jmp .loop
.return:
    ret

fatal:
    mov si, fatal_msg
    call puts
    jmp halt


message: db "It works!", 13, 10, 0
fatal_msg: db "Bootloader error", 13, 10, 0
loader_name: db "RASTALDRBIN"
found_msg: db "Found", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xaa55

