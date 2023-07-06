; Bochs debugging memo
;   Examine 11 bytes at 0x506: xp /11bm 0x0506
;   Examine 11 chars at 0x506: xp /11cm 0x0506
bits 16
org 0x7c00

%define breakpoint xchg bx, bx

    ; jump over bpb
    jmp  short start
    nop

    ; bpb
bpb:
    .oem_name:          times 8 db 0
    .bytes_per_sect:    dw 0
    .sect_per_clus:     db 0
    .rsvd_sect_count:   dw 0
    .fat_count:         db 0
    .root_ent_count:    dw 0
    .total_sectors16:   dw 0
    .media:             db 0
    .fat_size16:        dw 0
    .sect_per_track:    dw 0
    .num_heads:         dw 0
    .hidden_sect_count: dd 0
    .total_sectors32:   dd 0
    .drive_num:         db 0
    .reserved1:         db 0
    .bootsig:           db 0
    .volid:             dd 0
    .label:             times 11 db 0
    .fstype:            times 8 db 0
    .end:
%if bpb.end - bpb != (62 - 3)
%error "Wrong bpb size"
%endif

struc dir_entry
    .name:              resb 11
    .attrs:             resb 1
    .ignore:            resb 14
    .first_cluster:     resw 1
    .size:              resd 1
endstruc
%if dir_entry_size != 32
%error "Wrong dir_entry size"
%endif

    ; start of bootloader
start:
    cli
    cld

    ; reload cs so we're in [0x0000:0x7Cxx]
    jmp  0x0:start2

start2:
    ; reload segment registers
    xor  ax, ax
    mov  ds, ax
    mov  es, ax

    ; setup stack
    mov  ss, ax
    mov  sp, 0x7B00

    ; save dl (boot drive number)
    mov  [bpb.drive_num], dl

    ; usable conventional memory: 0x0500 - 0x7BFF (~29kb)
    ; calculate sector for root dir
root_dir_sects equ 0x0500
    mov  ax, [bpb.root_ent_count]
    shl  ax, 5           ; * 32
    add  ax, [bpb.bytes_per_sect]
    dec  ax
    xor  dx, dx                          ; divide ax:ax by [bytes_per_sect]
    div  word [bpb.bytes_per_sect]       ; result in ax
    mov  [root_dir_sects], ax

first_data_sector equ root_dir_sects + 2
    movzx ax, [bpb.fat_count]
    mul  word [bpb.fat_size16]           ; result in ax
    add  ax, [bpb.rsvd_sect_count]
    add  ax, [root_dir_sects]
    mov  [first_data_sector], ax

first_root_dir_sect equ first_data_sector + 2
    mov  ax, [first_data_sector]
    sub  ax, [root_dir_sects]
    mov  [first_root_dir_sect], ax

    ; load root dir's first sector
sector_buffer equ first_root_dir_sect + 2
    push ax
    push sector_buffer
    call load_sector

    ; walk root dir
    ; bx: pointer to current entry
    ; dx: root dir sector number
    mov  bx, sector_buffer
    mov  dx, [first_root_dir_sect]
walk_root:
    ; check end of entries
    cmp  byte [bx + dir_entry.name], 0
    je   .not_found

    ; compare filename
    push bx
    mov  cx, 11
    mov  si, bx
    mov  di, filename
    repe cmpsb
    pop  bx

    jcxz .found

    ; advance
    add  bx, dir_entry_size
    cmp  bx, [bpb.bytes_per_sect]
    je   .next_sector
    jmp  walk_root

.next_sector:
    cmp  dx, [root_dir_sects]
    je   .not_found
    inc  dx
    push dx                         ; save
    push dx
    call load_sector
    pop  dx

.not_found:
    push not_found_message
    call trace
    jmp  halt

.found:
    ; bx: pointer to dir_entry where the file was found
file_entry equ (sector_buffer + 512)
    mov  cx, dir_entry_size
    mov  si, bx
    mov  di, file_entry
    rep  movsb

; load fat into memory
; we assume the fat fits into 6kb (512 bytes sectors)
; and we align the buffer to 512 bytes for good measure
;   bx: remaining sectors to load
;   si: sector num
;   di; buffer pointer
fat_buffer equ ((file_entry + dir_entry_size + 511) / 512) * 512
    mov  bx, [bpb.fat_size16]
    mov  si, [bpb.rsvd_sect_count]
    mov  di, fat_buffer
load_fat:
    push si
    push di
    call load_sector
    dec  bx
    test bx, bx
    jz   .done
    inc  si
    add  di, [bpb.bytes_per_sect]
    jmp  load_fat

.done:

    ; read file into 0x7E00 - 0xFFFF (~32Kb max)
    ; ax: current_sector
    ; bx: current_cluster
    ; cx: sector counter
    ; dx:
    ; di: dest
    ; si:
    mov  di, 0x7E00
    mov  bx, [file_entry + dir_entry.first_cluster]
load_file:
    ; sect = ((current_cluster - 2) *
    ;   bpb.sect_per_clus) +
    ;   first_data_sector
    xor  dx, dx
    mov  ax, bx
    sub  ax, 2
    movzx cx, [bpb.sect_per_clus]
    mul  cx
    add  ax, [first_data_sector]

.load_sector:
    push ax             ; save
    push cx             ; save

    push ax             ; cluster number
    push di             ; buffer
    call load_sector

    pop  cx             ; restore
    pop  ax             ; restore
    dec  cx
    inc  ax  
    add  di, [bpb.bytes_per_sect]
    test cx, cx
    jnz  .load_sector

.next_cluster:
    push bx
    call next_cluster
    cmp  ax, 0x0FF7     ; 0xFF7: bad cluster, >= 0xFF8: eof
    jae  .eof
    mov  bx, ax

    jmp  load_file

.eof:
    ; jump into it
    jmp  0x7e00

halt:
    cli
    hlt
    jmp  $

; void trace(const char* str)
;   bp - 4      str
;   bp - 2      return address
;   bp          old bp
trace:
    push bp
    mov  bp, sp
    push bx
    push si

    mov  si, [bp + 4]

.output_char:
    cmp  byte [si], 0
    je   .break

    mov  ah, 0x0E
    mov  al, [si]
    xor  bh, bh
    mov  bl, 0x07
    int  0x10

    inc  si
    jmp  .output_char

.break:
    mov  al, 10
    int  0x10

    mov  al, 13
    int  0x10

    pop  si
    pop  bx
    pop  bp
    ret  2

; unsigned load_sector(void* buffer, unsigned lba)
;   tmp = lba / spt
;   sect = (lba % spt) + 1
;   head = tmp % heads
;   cyl = tmp / heads
;
;   stack layout
;   ============
;   bp + 6          num
;   bp + 4          buffer
;   bp + 2          return address
;   bp              old ebp
;   bp - 2          tmp
;   bp - 4          sect
;   bp - 6          head
;   bp - 8          cyl
;   bp - 10         retry count
load_sector:
push bp
    mov  bp, sp
    sub  sp, 10
    push bx

    ; tmp
    xor  dx, dx
    mov  ax, [bp + 6]
    div  word [bpb.sect_per_track]       ; ax: quotient, dx: remainder
    mov  [bp - 2], ax

    ; sect
    mov  ax, dx
    inc  ax
    mov  [bp - 4], ax

    ; head
    xor  dx, dx
    mov  ax, [bp - 2]
    div  word [bpb.num_heads]
    mov  word [bp - 6], dx

    ; cyl
    mov  [bp - 8], ax

    ; bios call (should be retried 3 times)
    mov  word [bp - 10], 0
.try_read:
    mov  ah, 0x02
    mov  al, 1               ; number of sectors to read
    mov  ch, [bp - 8]        ; cyl
    mov  cl, [bp - 4]        ; sector
    mov  dh, [bp - 6]        ; head
    mov  dl, [bpb.drive_num] ; drive
    mov  bx, [bp + 4]        ; es:buffer
    int  0x13
    test ah, ah
    jz   .success
    inc  word [bp - 10]
    cmp  word [bp - 10], 3
    je   .failure

    mov  ah, 0x00                ; reset disk
    int  0x13

    jmp  short .try_read

.failure:
    push io_error_message
    call trace
    jmp  halt

.success:
    pop  bx
    mov  sp, bp
    pop  bp
    ret  4

; unsigned next_cluster(unsigned cluster)
;   bp + 4          cluster
;   bp + 2          return address
;   bp              old bp
;   bx              fat_value
;   cx              fat_offset
;   dx              cluster
next_cluster:
    push bp
    mov  bp, sp
    push bx

    ; fat_offset = cluster + (cluster / 2)
    mov  dx, [bp + 4]
    mov  ax, dx
    shr  ax, 1
    add  ax, dx
    mov  cx, ax

    ; fat_value =
    ;   *(fat_buffer + fat_offset)
    mov  bx, fat_buffer
    add  bx, cx
    mov  ax, [bx]
    
    ; fat_value =
    ;   cluster & 0x1 ?
    ;       fat_value >> 4 :
    ;       fat_value & 0xFFF
    test dx, 0x1
    jz   .odd

    shr  ax, 4
    jmp  .return

.odd:
    and  ax, 0x0FFF

.return:
    pop  bx
    mov  sp, bp
    pop  bp
    ret 4

; data
io_error_message: db "ERR", 0
not_found_message: db "NTFND", 0

filename: db "BOOT1   BIN"

; boot signature
times 510 - ($ - $$) db 0
db 0x55, 0xAA

