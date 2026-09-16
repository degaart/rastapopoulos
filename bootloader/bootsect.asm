; Loads root-directory RASTALDR.BIN to 0800:0000 (physical 8000h)
; Maximum file size 32768 bytes, including sector rounding
;
; Buffers: 0600..21FF FAT (<=7168 bytes), 2200..31FF directory sector.
; Stack below 7A00h; code at 7C00h; loader 8000..FFFFh.
; Entry: DL=BIOS drive. Exit: CS=ES=0800h, IP=0, DS=SS=0,
; SP=7A00h, DL=boot drive, interrupts enabled, direction flag clear.
bits 16
cpu 8086
org 0x7c00
FAT equ 0x600
ROOT equ 0x2200
DATA equ 0x500
LEFT equ 0x502
FATEND equ 0x504

; Boot header including the entry jump, so fields are sector-relative.
struc BPB
    .jump:                  resb 3
    .oem_name:              resb 8
    .bytes_per_sector:      resw 1
    .sectors_per_cluster:   resb 1
    .reserved_sectors:      resw 1
    .fat_count:             resb 1
    .root_entries:          resw 1
    .total_sectors_16:      resw 1
    .media_descriptor:      resb 1
    .sectors_per_fat:       resw 1
    .sectors_per_track:     resw 1
    .head_count:            resw 1
    .hidden_sectors:        resd 1
    .total_sectors_32:      resd 1
    .boot_drive:            resb 1
    .reserved:              resb 1
    .extended_signature:    resb 1
    .volume_serial:         resd 1
    .volume_label:          resb 11
    .filesystem_type:       resb 8
endstruc

boot_sector:
    istruc BPB
        at BPB.jump, jmp short boot
                     nop
        at BPB.oem_name,              db 'RASTAOS '
        at BPB.bytes_per_sector,      dw 0
        at BPB.sectors_per_cluster,   db 0
        at BPB.reserved_sectors,      dw 0
        at BPB.fat_count,             db 0
        at BPB.root_entries,          dw 0
        at BPB.total_sectors_16,      dw 0
        at BPB.media_descriptor,      db 0
        at BPB.sectors_per_fat,       dw 0
        at BPB.sectors_per_track,     dw 0
        at BPB.head_count,            dw 0
        at BPB.hidden_sectors,        dd 0
        at BPB.total_sectors_32,      dd 0
        at BPB.boot_drive,            db 0
        at BPB.reserved,              db 0
        at BPB.extended_signature,    db 0
        at BPB.volume_serial,         dd 0
        at BPB.volume_label,          db 'RASTAOS    '
        at BPB.filesystem_type,       db 'FAT12   '
    iend
boot:
    cli
    xor ax,ax
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7a00
    sti
    cld
    mov bp,boot_sector          ; BPB structure offsets fit disp8 addressing
    mov [bp+BPB.boot_drive],dl

    ; Bound buffers. Valid sector sizes are powers of two, 512..4096.
    mov ax,[bp+BPB.bytes_per_sector]
    cmp ax,512
    jb fail
    cmp ax,4096
    ja fail
    mov dx,ax
    dec dx
    test ax,dx
    jnz fail
    ; Copy the BIOS floppy parameter table; update sector size and EOT.
    push ds
    lds si,[0x78]
    mov di,0x510
    mov cx,11
    rep movsb
    pop ds
    mov dl,[bp+BPB.sectors_per_track]
    mov [0x514],dl
    mov dx,ax
    mov cl,7
    shr dx,cl
    xor cx,cx
.size:
    shr dx,1
    jz .size_done
    inc cx
    jmp short .size
.size_done:
    mov [0x513],cl
    cli
    mov word [0x78],0x510
    mov [0x7a],ds
    sti
    cmp word [bp+BPB.hidden_sectors+2],0           ; no 32-bit disk addressing
    jne fail
    mul word [bp+BPB.sectors_per_fat]
    or dx,dx
    jnz fail
    cmp ax,ROOT-FAT
    ja fail
    add ax,FAT
    mov [FATEND],ax
    mov cx,[bp+BPB.sectors_per_fat]
    jcxz fail
    mov ax,[bp+BPB.reserved_sectors]               ; first FAT, not hardcoded LBA 1
    mov bx,FAT
.fat:
    call read
    loop .fat

    xor ax,ax
    mov al,[bp+BPB.fat_count]
    mul word [bp+BPB.sectors_per_fat]
    add ax,[bp+BPB.reserved_sectors]
    push ax                     ; root LBA
    mov ax,[bp+BPB.bytes_per_sector]
    mov cl,5
    shr ax,cl
    mov si,ax                   ; entries/sector
    mov ax,[bp+BPB.root_entries]
    or ax,ax
    jz fail
    dec ax
    xor dx,dx
    div si
    inc ax                      ; ceil(root entries / entries per sector)
    pop dx
    add ax,dx
    mov [DATA],ax
    mov ax,dx
    mov dx,[bp+BPB.root_entries]              ; total entries left to inspect
.root:
    mov bx,ROOT
    call read                   ; AX advances, BX advances by sector size
    mov di,ROOT
    mov si,[bp+BPB.bytes_per_sector]
.entry:
    cmp byte [di],0
    je fail
    test byte [di+11],0x18       ; skip volume labels, directories, LFNs
    jnz .next
    push si
    push di
    mov si,name
    mov cx,11
    repe cmpsb
    pop di
    pop si
    je found
.next:
    dec dx
    jz fail
    add di,32
    sub si,32
    jnz .entry
    jmp short .root
fail:
    mov ax,0x0e21               ; '!': invalid/unsupported, missing, or I/O
    mov bx,7
    int 0x10
.halt:
    cli
    hlt
    jmp short .halt
found:
    cmp word [di+30],0
    jne fail
    mov ax,[di+28]
    dec ax                      ; reject empty or >32768, round up safely
    cmp ax,32767
    ja fail
    xor dx,dx
    div word [bp+BPB.bytes_per_sector]
    inc ax
    mov [LEFT],ax              ; remaining file sectors
    mov si,[di+26]
    mov ax,0x800
    mov es,ax
    xor bx,bx
.cluster:
    mov ax,si
    sub ax,2
    cmp ax,0xfee                ; reject <2 and >=FF0h
    jae fail
    xor cx,cx
    mov cl,[bp+BPB.sectors_per_cluster]
    jcxz fail
    mul cx
    add ax,[DATA]
.sector:
    call read
    dec word [LEFT]
    jz launch
    loop .sector
    push bx                     ; destination offset
    mov bx,si
    shr bx,1
    add bx,si
    add bx,FAT
    inc bx
    cmp bx,[FATEND]             ; both bytes of packed entry must exist
    jae fail
    mov ax,[bx-1]
    pop bx
    test si,1
    jz .even
    mov cl,4
    shr ax,cl
.even:
    and ax,0xfff
    mov si,ax
    jmp short .cluster
launch:
    mov dl,[bp+BPB.boot_drive]
    jmp 0x800:0

; AX=volume-relative sector, ES:BX=buffer. Returns AX+1, BX+bytes/sector.
; Preserves CX,DX,SI,DI,BP; BIOS standard register preservation required.
; Single-sector transfers never cross a track or 64 KiB DMA boundary.
read:
    push cx
    push dx
    push si
    mov si,3
.retry:
    push ax
    push bx
    add ax,[bp+BPB.hidden_sectors]              ; hidden sector offset (16-bit floppy LBA)
    xor dx,dx
    div word [bp+BPB.sectors_per_track]
    mov cl,dl
    inc cl
    xor dx,dx
    div word [bp+BPB.head_count]
    mov ch,al
    ror ah,1
    ror ah,1
    or cl,ah                   ; cylinder bits 8..9 -> CL bits 6..7
    mov dh,dl
    mov dl,[bp+BPB.boot_drive]
    mov ax,0x201
    int 0x13
    pop bx
    pop ax
    jnc .ok
    push ax
    xor ax,ax
    mov dl,[bp+BPB.boot_drive]
    int 0x13
    pop ax
    dec si
    jnz .retry
    jmp fail
.ok:
    pop si
    pop dx
    pop cx
    inc ax
    add bx,[bp+BPB.bytes_per_sector]
    ret
name: db 'RASTALDRBIN'

times 510-($-$$) db 0
dw 0xaa55
