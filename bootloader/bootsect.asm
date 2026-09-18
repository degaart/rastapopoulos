; vim: tabstop=4 shiftwidth=4 expandtab nocindent autoindent:
; Loads RASTALDR.BIN at 800:0000 (physical 0x8000) and jumps to it.
; Some parts of the code assume 512-byte sectors, and I'm too lazy rn to fix that
bits    16
cpu     8086
org     0x7c00

    ; constants
    FAT12_FREE              equ 0xe5
    LOAD_SEGMENT            equ 0x800
    BAD_CLUSTER             equ 0xff7
    END_OF_CHAIN            equ 0xff8

    ; Work buffers
    VARS                    equ 0x500
struc V
    .cluster                resw 1
    .root_dir_lba           resw 1
    .root_dir_size_sect     resw 1
    .read_retry             resw 1
    .paragraphs_per_cluster resw 1
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
    jmp 0:setupsegs

setupsegs:
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7c00
    cld
    mov  [bpb.boot_drive], dl


    ; read first FAT
    mov ax, 1
    mov cx, [bpb.sectors_per_fat]
    cmp cx, 9
    je  bad
    mov bx, VARS+V.fat_buffer
    call read_sectors
    test ax, ax
    jnz bad

    ; read root directory
    ; lba = reserved_sectors + (fat_count * sectors_per_fat)
    ; sectors = (root_entries) * 32 / bytes_per_sector
    ; this is wrong, the sector count should be aligned up
    ; the correct formula is:
    ;   sectors = ((root_entries * 32) + bytes_per_sector - 1) / bytes_per_sector
    mov ax, [bpb.root_entries]
    mov cl, 5
    shl ax, cl                          ; ax *= 32
    xor dx, dx
    div word [bpb.bytes_per_sector]     ; ax /= bytes_per_sector
    mov cx, ax
    mov [VARS+V.root_dir_size_sect], ax

    xor ax, ax
    mov al, [bpb.fat_count]
    mul word [bpb.sectors_per_fat]      ; ds:ax = fat_count * sectors_per_fat
    add ax, [bpb.reserved_sectors]      ; ax = lba
    mov [VARS+V.root_dir_lba], ax

    mov bx, VARS+V.root_buffer
    call read_sectors
    test ax, ax
    jnz bad

    ; find loader
    mov di, VARS+V.root_buffer
find_ldr:
    cmp byte [di], 0                ; end of root dir entries
    jz bad
    cmp byte [di], FAT12_FREE       ; free entry, skip
    je .next

    ; compare with loader name
    push di
    mov si, loader_name
    mov cx, 11
    repe cmpsb
    pop di
    je .found
 .next:
    add di, Dirent_size
    ; dec dx
    jnz find_ldr

 .found:
    ; directory entry in di
    ; we don't load if first cluster > 64k, or file size is > 64k
    mov ax, [di+Dirent.first_cluster_hi]
    mov dx, [di+Dirent.file_size+2]
    or ax, dx
    jnz bad

    ; paragraphs_per_cluster = (sectors_per_cluster * bytes_per_sector) / 16
    mov al, [bpb.sectors_per_cluster]               ; ax = 0 before this
    ; xor dx, dx                                    ; not needed, dx is already 0
    mul word [bpb.bytes_per_sector]
    mov cl, 4
    shr ax, cl
    mov [VARS+V.paragraphs_per_cluster], ax

    ; current cluster
    mov ax, [di+Dirent.first_cluster_low]
    mov word [VARS+V.cluster], ax

    ; bx stays the same
    ; but es is incremented each loop
    mov ax, LOAD_SEGMENT
    mov es, ax

load_file:
    ; ax: cluster
    ; es:bx: buffer
    mov ax, [VARS+V.cluster]
    xor bx, bx
    call read_cluster
    test ax, ax
    jnz bad

    ; inc es
    mov ax, es
    add ax, [VARS+V.paragraphs_per_cluster]
    mov es, ax

    ; next cluster
    ; fat_offset = cluster + (cluster / 2)
    ; current cluster is even: next cluster = fat_buffer[fat_offset] & 0xfff
    ; current cluster is odd:  next cluster = fat_buffer[fat_offset] >> 4
    ; validate next cluster >= 2 && next_cluster != 0xff7

    ; fat offset
    mov ax, [VARS+V.cluster]
    mov si, ax
    mov cl, 1
    shr si, cl
    add si, ax

    test ax, 1
    jz .even

    ; odd cluster
    mov ax, [VARS+V.fat_buffer+si]
    mov cl, 4
    shr ax, cl
    jmp .check

.even:
    ; even cluster
    mov ax, [VARS+V.fat_buffer+si]
    and ax, 0xfff

.check:
    ; This lacks a check for reserved clusters (0xFF0–0xFF6)
    cmp ax, 2
    jb bad
    cmp ax, BAD_CLUSTER                 ; bad sector
    je bad
    cmp ax, END_OF_CHAIN                ; end of cluster chain
    jae done_loading

    mov [VARS+V.cluster], ax
    jmp load_file

bad:
    mov  al, 'B'
    mov  ah, 0xe
    mov  bx, 0x7
    int  0x10
    jmp  halt

done_loading:
    jmp LOAD_SEGMENT:0

; load cluster from boot_drive
; AX: cluster number
; ES:BX: destination buffer
; On return: AX: status (0 = success)
; Preserves all registers except AX
read_cluster:
    ; LBA=FirstDataLBA+((cluster−2)×SectorsPerCluster)
    ; FirstDataLBA=RootDirLba+RootDirSizeSect
    push cx

    sub ax, 2
    xor dx, dx
    xor cx, cx
    mov cl, [bpb.sectors_per_cluster]
    mul cx

    add ax, [VARS+V.root_dir_lba]
    add ax, [VARS+V.root_dir_size_sect]
    call read_sectors

    pop cx
    ret

; read multiple disk sectors from boot_drive
; AX: starting lba
; CX: sector count
; ES:BX: destination buffer
; On return: AX: status (0 = success)
; Preserve all registers except AX
read_sectors:
    push es
    push cx
    push dx
    push si

    mov si, ax          ; si = current lba

.loop:
    test cx, cx
    jz .return

    mov ax, si
    call read_sector
    test ax, ax
    jnz .return

    dec cx
    inc si

    push ax
    push cx
    mov dx, [bpb.bytes_per_sector]
    mov cl, 4
    shr dx, cl         ; divide by 16

    mov ax, es
    add ax, dx
    mov es, ax
    pop cx
    pop ax

    jmp .loop
.return:
    pop si
    pop dx
    pop cx
    pop es
    ret

; read one sector from boot_drive
; AX: lba of sector to read
; ES:BX: destination
; On return: AX: status (0 = success)
; retries 3 times in case of read error
; Assumes int 0x13 does not trash any registers except AX
read_sector:
    ; C = LBA / (HeadCount * Spt)
    ; H = (LBA / Spt) % HeadCount
    ; S = (LBA % Spt) + 1
    ; CX = ((C & 0xff) << 8)|((C & 0x300) >> 2)|S
    push cx
    push dx

    mov  word [VARS+V.read_retry], 3

.loop:
    mov  dx, [VARS+V.read_retry]
    cmp  dx, 0
    je   .return

    xor  dx, dx
    div  word [bpb.sectors_per_track]
    ; AX = LBA / sectors_per_track
    ; DX = LBA % sectors_per_track

    inc  dl
    mov  cl, dl                  ; sector, 1-based

    xor  dx, dx
    div  word [bpb.head_count]
    ; AX = cylinder
    ; DX = head

    mov  dh, dl
    mov  ch, al                  ; cylinder bits 0–7
    mov  al, ah                  ; cylinder bits 8–9
    and  al, 0x03
    push cx
    mov  cl, 6
    shl  al, cl
    pop  cx
    or   cl, al

    mov  ah, 2          ; func
    mov  al, 1          ; sect count
    mov  dl, [bpb.boot_drive]
    int  0x13
    jnc  .return        ; pop does not change flags

    xor  ah, ah
    int  0x13

    mov  dx, [VARS+V.read_retry]
    dec  dx
    mov  [VARS+V.read_retry], dx
    jmp  .loop

.return:
    pop dx
    pop cx
    mov al, ah
    xor ah, ah
    ret

halt:
    jmp  halt

loader_name: db "RASTALDRBIN"

times 510 - ($ - $$) db 0
dw 0xaa55

