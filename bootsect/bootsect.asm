; Rastapopoul OS 0.09
;
; Bootsector that loads BOOTLDR from floppy
; and executes it
;
; Memory layout at boot:
; 0x500 - 0x5FF	: kernel params
; 0x600 - 0x6FF : bootloader work area
; 0x700 - 0x7BFE: copy of FAT
; 0x7BFF		: bootloader stack
;
;
; Conversion formulas
; 	lba = ((cyl*heads)+heads) * spt + (sector - 1)
; 	cyl = lba / (spt * heads)
; 	head = (lba / spt) % heads
; 	sector = (lba % spt) + 1
;
bits 16
org 0x7C00
		%include 'bootsect.inc'

		current_sect equ 0x502 	; word
		read_sectors equ 0x504	; word (number of sectors read so far)
		datasect_start equ 0x508; word (starting sector of data on disk)
		
		spt equ 0x7C18			; sectors per track (word)
		spf equ 0x7C16			; sectors per fat (word)
		heads equ 0x7C1A		; head count (word)
		rdirentcnt equ 0x7C11	; root dir entry count (word)
		rsectcount equ 0x7C0E	; reserved sector count (word)
		
		workmem equ 0x600		; bootloader work mem (512 bytes)
		fat equ 0x800			; copy of FAT (~6144 bytes)
		load_area equ 0x7E00	; loading area for second-stage bootloader
		

		; jump to start of bootsector
		jmp short start
		
		; BPB
		times 59 db 0

start:
		; dl=bios boot drive number
		;mov byte [0x500],dl
		mov ax,cs
		mov ds,ax
		mov es,ax
		mov ss,ax
		mov sp,0x7BFF

		; When booted from FAT12:
		; 0x7C0B	bytes/sector. Always 512 for floppy (word)
		; 0x7C0D	sectors/cluster. Usually 1 for floppy (byte)
		; 0x7C0E	reserved sectors count+boot record. Usually 1 for FAT12/FAT16 (word)
		; 0x7C10	fat count. Usually 2 (word)
		; 0x7C11	dir entry count for root dir (32b/entry) (word)
		; 0x7C13	total sector count. Always 2880 for floppy If 0, refer to large sector count (word)
		; 0x7C16	sectors/fat (word)
		; 0x7C18	sectors/track. Usually 18 for floppy (word)
		; 0x7C1A	head count. Usually 2 for floppy (word)
		; 0x7C1C	hidden sectors count. Usually 0 for floppy (DWORD)
		; 0x7C20	large sector count (DWORD)
		
		; save boot device number
		mov [boot_device], dl
		
		; check some assumptions
		cmp word [0x7C0B], 512
		jne .unsupported
		cmp byte [0x7C0D], 1
		je .preload_fat
	.unsupported:
		mov dx, str.unsupported
		call write_string
		jmp halt
		

		; preload fat into conventional memory
		; WARNING: Do not do this for FAT16!
	.preload_fat:
		mov bx, fat
		mov ax, 2
	.copyfatloop:
		cmp ax, [spf]
		je .endcopyfat
		push ax
		push bx
		call read_lsect
	
		pop bx
		pop ax
		inc ax
		add bx,512
		jmp short .copyfatloop

	.endcopyfat:

		; calculate root dir sector
		; (number of fats * sectors/fat) + hidden sectors + reserved sectors
		; assume number of fats=2
		; The correct value should be 20
		mov ax,[spf]
		shl ax,1
		add ax,[0x7C1C]
		add ax,[0x7C0E]
		inc ax

		; So we load fat direntries into conventional memory at 0x600
		; one 512-byte sector at a time
		; beginning with root dir sector, and so on
		mov [current_sect], ax
		mov word [read_sectors], 0
	
	.readloop:
		;breakpoint
		; check if we reached end of fat entries
		mov ax, [read_sectors]
		cmp ax, [spf]
		je .notfound

		; read sector into memory at workmem
		mov ax, [current_sect]
		mov bx, workmem
		call read_lsect
		
		; read each entry (bx is preserved by read_lsect)
		;mov bx, workmem			; entry pointer
	
	.begin_check_entry:
		cmp byte [bx], 0
		jz .notfound

		mov si, str.bootldr ; bootldr string
		mov di, bx			; filename in dir entry
	.check_entry:
		cmp si, str.end_bootldr
		je .entry_found
		
		mov al, [si]
		cmp al, [di]
		jne .read_next_entry
		
		inc si
		inc di
		jmp short .check_entry
	.read_next_entry:
		add bx,32
		cmp bx,workmem+0xFF
		jae .read_next_sector
		jmp short .begin_check_entry
	
	.read_next_sector:
		add word [read_sectors],1
		add byte [current_sect],1
		jmp short .readloop
	.notfound:
		mov di,str.bootldr
		call write_string
		jmp halt

	.entry_found:
		; fat entry found at bx
		; first cluster: bx+26 (word)
		; size: bx+28 (dword)
		add bx, 26
		mov ax, [bx]
		mov bx, load_area
		call read_file

		; check magic of bootldr
		mov eax, [load_area+2]
		cmp eax, 0x59415442			; BTAY
		jne .bad_magic
		
		; bootldr expects to start at 0x7e00
		; we assume cs is 0
		mov di, str.loading
		call write_string
		jmp load_area

	.bad_magic:
		mov di, str.bad_magic
		call write_string
		jmp halt

read_file:
		; read file from disk
		; bx: buffer
		; ax: first cluster of file, as reported by it's directory entry
		mov [current_sect],ax
		
		; unsigned root_dir_sectors = ((*direntcount * 32) + (512-1)) / 512;
		; unsigned first_data_sector = *rsectcount + (2 * *spf) + root_dir_sectors;
		mov ax, [rdirentcnt]
		shl ax, 5
		add ax, 511
		shr ax, 9
		mov cx, [spf]
		shl cx, 1
		add cx, [rsectcount]
		add cx, ax
		mov [datasect_start],cx
		
		; Read sectors
		; read_lsect(current_buffer, (current_sector-2) + first_data_sector + 1);
	.read_sectors:
		mov ax, [current_sect]
		dec ax
		dec ax
		add ax, [datasect_start]
		inc ax
		call read_lsect
		
		; unsigned fat_offset = current_sector + (current_sector / 2);
		; uint16_t next_sector = *((uint16_t*)(fat_table+fat_offset));
		mov di, [current_sect]
		mov ax, di								; ax: current_sector
		shr di, 1
		add di, ax
		mov dx, [fat+di]						; dx: next_sector
		
		;if(current_sector & 0x1) { /* Odd */
		;	/* Keep high order 12 bits */
		;	next_sector = next_sector >> 4;
		;} else {
		;	/* Keep low order 12 bits */
		;	next_sector = next_sector & 0x0FFF;
		;}
		and ax, 1
		jnz .odd_sector
		and dx, 0x0FFF
		jmp short .test_end_sector
		
	.odd_sector:
		shr dx, 4
	
	.test_end_sector:
		;if(next_sector >= 0xFF8)
		;	break;
		cmp dx, 0xFF8
		jae .done
		
		;current_sector = next_sector;
        ;current_buffer += 512;
        mov [current_sect], dx
        add bx, 512
        jmp short .read_sectors
    
    .done:
    	ret
write_string:
		; write null terminated string and advance cursor
		; di: string to write
		push bx
		xor bx, bx
	.loop:
		mov al,[di]
		test al,al
		jz .exit
		mov ah,0xE
		;xor bh,bh
		int 0x10
		inc di
		jmp short .loop
	.exit:
		mov al, 0x0D
		int 0x10
		mov al, 0x0A
		int 0x10

		pop bx
		ret
		
read_lsect:
		; read 1 sector from drive 0
		; and store in bx
		; ax: linear sector number (1-based)
		
		; Formulas:
		; 	temp = (lsect -1)/spt
		;	s = ((sect-1)%spt)+1
		;	h = temp % heads
		;	c = temp / heads
		dec ax
		push ax
		
		xor dx,dx
		div word [spt]
		add dx,1
		mov cx,dx			; sector

		pop ax
		xor dx,dx
		div word [spt]		; ax: temp
		
		xor dx,dx
		div word [heads]	; ax: cyl, dx: head
		
		call read_chs
		ret

read_chs:
		; read 1 sector from drive
		; and store in [bx]
		; cx: sector number to read
		; dx: head
		; ax: cylinder
		push dx
		
		mov ch, al				; cylinder & 0xFF
		
		mov dx, ax
		shr dx, 2
		and dx, 0x00C0
		or  cl, dl				; sector | ((cyl >> 2) & 0xC0)
		
		pop dx
		mov dh, dl				; head
		mov dl, [boot_device] 	; drive number
		mov ax, 0x0201			; Function=2, sector count=1
		int 0x13
		jc .ioerror
		ret
	.ioerror:
		mov dx, str.ioerror
		call write_string
		; need not call halt

halt:
		jmp short halt

str:
		.ioerror: db 'ERR',0
		.bootldr: db 'BOOTLDR    '
		.end_bootldr:
		.notfound: db 'missing',0				; to get this string, use .bootldr! ahahahahaha
		.unsupported: db 'Unsuported',0
		.loading: db 'Loading',0
		.bad_magic: db 'Bad magic',0

		; padding for bios
		times 510-($-$$) db 0
		db 0x55,0xAA
