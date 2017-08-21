	.file	"vfs.c"
	.intel_syntax noprefix
	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.type	is_pow2, @function
is_pow2:
.LFB0:
	.file 1 "../../common/util.h"
	.loc 1 45 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	.loc 1 46 0
	cmp	DWORD PTR [ebp+8], 0
	je	.L2
	.loc 1 46 0 is_stmt 0 discriminator 1
	mov	eax, DWORD PTR [ebp+8]
	dec	eax
	and	eax, DWORD PTR [ebp+8]
	test	eax, eax
	jne	.L2
	.loc 1 46 0 discriminator 3
	mov	eax, 1
	jmp	.L3
.L2:
	.loc 1 46 0 discriminator 4
	mov	eax, 0
.L3:
	.loc 1 46 0 discriminator 6
	and	eax, 1
	.loc 1 47 0 is_stmt 1 discriminator 6
	pop	ebp
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE0:
	.size	is_pow2, .-is_pow2
	.type	log2, @function
log2:
.LFB1:
	.loc 1 50 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	sub	esp, 16
	.loc 1 59 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax
	or	DWORD PTR [ebp+8], eax
	.loc 1 60 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 2
	or	DWORD PTR [ebp+8], eax
	.loc 1 61 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 4
	or	DWORD PTR [ebp+8], eax
	.loc 1 62 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 8
	or	DWORD PTR [ebp+8], eax
	.loc 1 63 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 16
	or	DWORD PTR [ebp+8], eax
	.loc 1 65 0
	mov	edx, DWORD PTR [ebp+8]
	mov	eax, edx
	add	eax, eax
	add	eax, edx
	lea	ecx, [0+eax*8]
	add	eax, ecx
	sal	eax, 2
	add	eax, edx
	add	eax, eax
	add	eax, edx
	sal	eax, 2
	add	eax, edx
	mov	ecx, eax
	sal	ecx, 7
	add	eax, ecx
	add	eax, eax
	add	eax, edx
	lea	ecx, [0+eax*8]
	add	eax, ecx
	sal	eax, 2
	add	eax, edx
	add	eax, eax
	add	eax, edx
	add	eax, eax
	add	eax, edx
	sal	eax, 2
	add	eax, edx
	shr	eax, 27
	mov	eax, DWORD PTR MultiplyDeBruijnBitPosition.1138[0+eax*4]
	mov	DWORD PTR [ebp-4], eax
	.loc 1 66 0
	mov	eax, DWORD PTR [ebp-4]
	.loc 1 67 0
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE1:
	.size	log2, .-log2
	.type	next_pow2, @function
next_pow2:
.LFB2:
	.loc 1 70 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	.loc 1 71 0
	dec	DWORD PTR [ebp+8]
	.loc 1 72 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax
	or	DWORD PTR [ebp+8], eax
	.loc 1 73 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 2
	or	DWORD PTR [ebp+8], eax
	.loc 1 74 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 4
	or	DWORD PTR [ebp+8], eax
	.loc 1 75 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 8
	or	DWORD PTR [ebp+8], eax
	.loc 1 76 0
	mov	eax, DWORD PTR [ebp+8]
	shr	eax, 16
	or	DWORD PTR [ebp+8], eax
	.loc 1 77 0
	inc	DWORD PTR [ebp+8]
	.loc 1 78 0
	mov	eax, DWORD PTR [ebp+8]
	.loc 1 79 0
	pop	ebp
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE2:
	.size	next_pow2, .-next_pow2
	.local	initrd
	.comm	initrd,4,4
	.local	file_descriptors
	.comm	file_descriptors,640,32
	.local	next_fd
	.comm	next_fd,4,4
	.type	getsize, @function
getsize:
.LFB3:
	.file 2 "vfs.c"
	.loc 2 39 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	sub	esp, 16
	.loc 2 40 0
	mov	DWORD PTR [ebp-4], 0
	.loc 2 42 0
	mov	DWORD PTR [ebp-12], 1
	.loc 2 44 0
	mov	DWORD PTR [ebp-8], 11
	jmp	.L10
.L11:
	.loc 2 45 0 discriminator 3
	mov	eax, DWORD PTR [ebp-8]
	lea	edx, [eax-1]
	mov	eax, DWORD PTR [ebp+8]
	add	eax, edx
	mov	al, BYTE PTR [eax]
	movsx	eax, al
	sub	eax, 48
	imul	eax, DWORD PTR [ebp-12]
	add	DWORD PTR [ebp-4], eax
	.loc 2 44 0 discriminator 3
	dec	DWORD PTR [ebp-8]
	sal	DWORD PTR [ebp-12], 3
.L10:
	.loc 2 44 0 is_stmt 0 discriminator 1
	cmp	DWORD PTR [ebp-8], 0
	jne	.L11
	.loc 2 47 0 is_stmt 1
	mov	eax, DWORD PTR [ebp-4]
	.loc 2 48 0
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE3:
	.size	getsize, .-getsize
	.section	.rodata
.LC0:
	.string	"data->mode == O_RDONLY"
.LC1:
	.string	"vfs.c"
	.align 4
.LC2:
	.string	"fd_index < sizeof(file_descriptors) / sizeof(file_descriptors[0])"
	.text
	.type	handle_open, @function
handle_open:
.LFB4:
	.loc 2 64 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	sub	esp, 40
	.loc 2 65 0
	mov	eax, DWORD PTR [ebp+8]
	add	eax, 28
	mov	DWORD PTR [ebp-16], eax
	.loc 2 66 0
	mov	eax, DWORD PTR [ebp-16]
	mov	eax, DWORD PTR [eax]
	cmp	eax, 1
	je	.L14
	.loc 2 66 0 is_stmt 0 discriminator 1
	push	OFFSET FLAT:.LC0
	push	66
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1174
	call	__assertion_failed
	add	esp, 16
.L14:
	.loc 2 68 0 is_stmt 1
	mov	eax, DWORD PTR initrd
	mov	DWORD PTR [ebp-12], eax
.L20:
.LBB2:
	.loc 2 70 0
	mov	eax, DWORD PTR [ebp-12]
	mov	al, BYTE PTR [eax]
	test	al, al
	je	.L22
	.loc 2 73 0
	mov	eax, DWORD PTR [ebp-12]
	add	eax, 124
	sub	esp, 12
	push	eax
	call	getsize
	add	esp, 16
	mov	DWORD PTR [ebp-20], eax
	.loc 2 74 0
	mov	eax, DWORD PTR [ebp-16]
	lea	edx, [eax+8]
	mov	eax, DWORD PTR [ebp-12]
	sub	esp, 8
	push	edx
	push	eax
	call	strcmp
	add	esp, 16
	test	eax, eax
	jne	.L17
.LBB3:
	.loc 2 76 0
	mov	eax, DWORD PTR next_fd
	lea	edx, [eax+1]
	mov	DWORD PTR next_fd, edx
	mov	DWORD PTR [ebp-24], eax
	.loc 2 77 0
	mov	eax, DWORD PTR [ebp-24]
	cmp	eax, 31
	jbe	.L18
	.loc 2 77 0 is_stmt 0 discriminator 1
	push	OFFSET FLAT:.LC2
	push	77
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1174
	call	__assertion_failed
	add	esp, 16
.L18:
	.loc 2 79 0 is_stmt 1
	mov	edx, DWORD PTR [ebp-24]
	mov	eax, edx
	sal	eax, 2
	add	eax, edx
	sal	eax, 2
	add	eax, OFFSET FLAT:file_descriptors
	mov	DWORD PTR [ebp-28], eax
	.loc 2 80 0
	mov	eax, DWORD PTR [ebp+8]
	mov	edx, DWORD PTR [eax+12]
	mov	eax, DWORD PTR [ebp-28]
	mov	DWORD PTR [eax], edx
	.loc 2 81 0
	mov	eax, DWORD PTR [ebp-28]
	mov	DWORD PTR [eax+4], 0
	.loc 2 82 0
	mov	eax, DWORD PTR [ebp-16]
	mov	edx, DWORD PTR [eax]
	mov	eax, DWORD PTR [ebp-28]
	mov	DWORD PTR [eax+8], edx
	.loc 2 83 0
	mov	eax, DWORD PTR [ebp-12]
	lea	edx, [eax+512]
	mov	eax, DWORD PTR [ebp-28]
	mov	DWORD PTR [eax+12], edx
	.loc 2 84 0
	mov	eax, DWORD PTR [ebp-28]
	mov	edx, DWORD PTR [ebp-20]
	mov	DWORD PTR [eax+16], edx
	.loc 2 86 0
	mov	eax, DWORD PTR [ebp+12]
	mov	edx, DWORD PTR [ebp-24]
	mov	DWORD PTR [eax], edx
	.loc 2 87 0
	mov	eax, 4
	jmp	.L19
.L17:
.LBE3:
	.loc 2 91 0
	mov	eax, DWORD PTR [ebp-20]
	add	eax, 511
	and	eax, -512
	add	eax, 512
	.loc 2 90 0
	add	DWORD PTR [ebp-12], eax
.LBE2:
	.loc 2 69 0
	jmp	.L20
.L22:
.LBB4:
	.loc 2 71 0
	nop
.LBE4:
	.loc 2 93 0
	mov	eax, DWORD PTR [ebp+12]
	mov	DWORD PTR [eax], -1
	.loc 2 94 0
	mov	eax, 4
.L19:
	.loc 2 95 0
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE4:
	.size	handle_open, .-handle_open
	.section	.rodata
.LC3:
	.string	"!\"Not implemented yet\""
	.text
	.type	handle_close, @function
handle_close:
.LFB5:
	.loc 2 98 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	sub	esp, 8
	.loc 2 99 0
	push	OFFSET FLAT:.LC3
	push	99
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1185
	call	__assertion_failed
	add	esp, 16
	.loc 2 100 0
	mov	eax, 0
	.loc 2 101 0
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE5:
	.size	handle_close, .-handle_close
	.type	handle_read, @function
handle_read:
.LFB6:
	.loc 2 104 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	sub	esp, 24
	.loc 2 105 0
	mov	eax, DWORD PTR [ebp+8]
	add	eax, 28
	mov	DWORD PTR [ebp-16], eax
	.loc 2 106 0
	mov	eax, DWORD PTR [ebp-16]
	mov	edx, DWORD PTR [eax]
	mov	eax, edx
	sal	eax, 2
	add	eax, edx
	sal	eax, 2
	add	eax, OFFSET FLAT:file_descriptors
	mov	DWORD PTR [ebp-20], eax
	.loc 2 108 0
	mov	eax, DWORD PTR [ebp-20]
	mov	edx, DWORD PTR [eax+16]
	mov	eax, DWORD PTR [ebp-20]
	mov	eax, DWORD PTR [eax+4]
	sub	edx, eax
	mov	eax, edx
	mov	DWORD PTR [ebp-24], eax
	.loc 2 109 0
	cmp	DWORD PTR [ebp-24], 0
	jne	.L26
	.loc 2 110 0
	mov	eax, DWORD PTR [ebp+12]
	mov	DWORD PTR [eax], -1
	.loc 2 111 0
	mov	eax, 4
	jmp	.L27
.L26:
	.loc 2 114 0
	mov	eax, DWORD PTR [ebp-16]
	mov	eax, DWORD PTR [eax+4]
	mov	DWORD PTR [ebp-12], eax
	.loc 2 115 0
	mov	eax, DWORD PTR [ebp-12]
	cmp	eax, DWORD PTR [ebp-24]
	jbe	.L28
	.loc 2 116 0
	mov	eax, DWORD PTR [ebp-24]
	mov	DWORD PTR [ebp-12], eax
.L28:
	.loc 2 119 0
	mov	eax, DWORD PTR [ebp-20]
	mov	edx, DWORD PTR [eax+12]
	mov	eax, DWORD PTR [ebp-20]
	mov	eax, DWORD PTR [eax+4]
	add	eax, edx
	.loc 2 118 0
	sub	esp, 4
	push	DWORD PTR [ebp-12]
	push	eax
	push	DWORD PTR [ebp+12]
	call	memcpy
	add	esp, 16
	.loc 2 122 0
	mov	edx, DWORD PTR [ebp-12]
	mov	eax, DWORD PTR [ebp+12]
	mov	DWORD PTR [eax], edx
	.loc 2 123 0
	mov	eax, DWORD PTR [ebp-12]
	add	eax, 4
.L27:
	.loc 2 124 0
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE6:
	.size	handle_read, .-handle_read
	.type	handle_write, @function
handle_write:
.LFB7:
	.loc 2 127 0
	.cfi_startproc
	push	ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	mov	ebp, esp
	.cfi_def_cfa_register 5
	sub	esp, 8
	.loc 2 128 0
	push	OFFSET FLAT:.LC3
	push	128
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1198
	call	__assertion_failed
	add	esp, 16
	.loc 2 129 0
	mov	eax, 0
	.loc 2 130 0
	leave
	.cfi_restore 5
	.cfi_def_cfa 4, 4
	ret
	.cfi_endproc
.LFE7:
	.size	handle_write, .-handle_write
	.section	.rodata
.LC4:
	.string	"Opening VFS port"
.LC5:
	.string	"Failed to open VFS port"
.LC6:
	.string	"mmap_ret != NULL"
.LC7:
	.string	"ret != -1"
.LC8:
	.string	"VFS started"
.LC9:
	.string	"msgrecv failed"
.LC10:
	.string	"msgsend failed"
	.text
	.globl	main
	.type	main, @function
main:
.LFB8:
	.loc 2 133 0
	.cfi_startproc
	lea	ecx, [esp+4]
	.cfi_def_cfa 1, 0
	and	esp, -16
	push	DWORD PTR [ecx-4]
	push	ebp
	.cfi_escape 0x10,0x5,0x2,0x75,0
	mov	ebp, esp
	push	ecx
	.cfi_escape 0xf,0x3,0x75,0x7c,0x6
	sub	esp, 52
	.loc 2 135 0
	push	OFFSET FLAT:.LC4
	push	135
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__log
	add	esp, 16
	.loc 2 136 0
	sub	esp, 12
	push	2
	call	port_open
	add	esp, 16
	mov	DWORD PTR [ebp-16], eax
	.loc 2 137 0
	cmp	DWORD PTR [ebp-16], 0
	jns	.L32
	.loc 2 138 0
	push	OFFSET FLAT:.LC5
	push	138
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__log
	add	esp, 16
	call	abort
.L32:
	.loc 2 142 0
	call	initrd_get_size
	mov	DWORD PTR [ebp-20], eax
	.loc 2 143 0
	mov	eax, OFFSET FLAT:__END__
	add	eax, 4095
	and	eax, -4096
	mov	DWORD PTR [ebp-24], eax
	.loc 2 144 0
	mov	eax, DWORD PTR [ebp-20]
	add	eax, 4095
	and	eax, -4096
	sub	esp, 4
	push	3
	push	eax
	push	DWORD PTR [ebp-24]
	call	mmap
	add	esp, 16
	mov	DWORD PTR [ebp-28], eax
	.loc 2 145 0
	cmp	DWORD PTR [ebp-28], 0
	jne	.L33
	.loc 2 145 0 is_stmt 0 discriminator 1
	push	OFFSET FLAT:.LC6
	push	145
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__assertion_failed
	add	esp, 16
.L33:
	.loc 2 147 0 is_stmt 1
	sub	esp, 8
	push	DWORD PTR [ebp-20]
	push	DWORD PTR [ebp-28]
	call	initrd_copy
	add	esp, 16
	mov	DWORD PTR [ebp-16], eax
	.loc 2 148 0
	cmp	DWORD PTR [ebp-16], -1
	jne	.L34
	.loc 2 148 0 is_stmt 0 discriminator 1
	push	OFFSET FLAT:.LC7
	push	148
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__assertion_failed
	add	esp, 16
.L34:
	.loc 2 150 0 is_stmt 1
	mov	eax, DWORD PTR [ebp-24]
	mov	DWORD PTR initrd, eax
	.loc 2 153 0
	mov	eax, DWORD PTR [ebp-20]
	add	eax, 4095
	and	eax, -4096
	mov	edx, eax
	mov	eax, DWORD PTR [ebp-28]
	add	eax, edx
	add	eax, 4095
	and	eax, -4096
	mov	DWORD PTR [ebp-24], eax
	.loc 2 154 0
	sub	esp, 4
	push	3
	push	4096
	push	DWORD PTR [ebp-24]
	call	mmap
	add	esp, 16
	mov	DWORD PTR [ebp-32], eax
	.loc 2 156 0
	mov	eax, DWORD PTR [ebp-32]
	add	eax, 4095
	and	eax, -4096
	mov	DWORD PTR [ebp-24], eax
	.loc 2 157 0
	sub	esp, 4
	push	3
	push	4096
	push	DWORD PTR [ebp-24]
	call	mmap
	add	esp, 16
	mov	DWORD PTR [ebp-36], eax
	.loc 2 160 0
	push	OFFSET FLAT:.LC8
	push	160
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__log
	add	esp, 16
.L43:
.LBB5:
	.loc 2 164 0
	lea	eax, [ebp-48]
	push	eax
	push	4096
	push	DWORD PTR [ebp-32]
	push	2
	call	msgrecv
	add	esp, 16
	mov	DWORD PTR [ebp-40], eax
	.loc 2 168 0
	cmp	DWORD PTR [ebp-40], 0
	je	.L35
	.loc 2 169 0
	push	OFFSET FLAT:.LC9
	push	169
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__log
	add	esp, 16
	call	abort
.L35:
	.loc 2 173 0
	mov	eax, DWORD PTR [ebp-36]
	add	eax, 28
	mov	DWORD PTR [ebp-44], eax
	.loc 2 175 0
	sub	esp, 4
	push	4096
	push	0
	push	DWORD PTR [ebp-36]
	call	memset
	add	esp, 16
	.loc 2 176 0
	mov	eax, DWORD PTR [ebp-32]
	mov	eax, DWORD PTR [eax+20]
	cmp	eax, 2
	je	.L37
	cmp	eax, 2
	ja	.L38
	cmp	eax, 1
	je	.L39
	jmp	.L36
.L38:
	cmp	eax, 3
	je	.L40
	cmp	eax, 4
	je	.L41
	jmp	.L36
.L39:
	.loc 2 178 0
	sub	esp, 8
	push	DWORD PTR [ebp-44]
	push	DWORD PTR [ebp-32]
	call	handle_open
	add	esp, 16
	mov	DWORD PTR [ebp-12], eax
	.loc 2 179 0
	jmp	.L36
.L37:
	.loc 2 181 0
	sub	esp, 8
	push	DWORD PTR [ebp-44]
	push	DWORD PTR [ebp-32]
	call	handle_close
	add	esp, 16
	mov	DWORD PTR [ebp-12], eax
	.loc 2 182 0
	jmp	.L36
.L40:
	.loc 2 184 0
	sub	esp, 8
	push	DWORD PTR [ebp-44]
	push	DWORD PTR [ebp-32]
	call	handle_read
	add	esp, 16
	mov	DWORD PTR [ebp-12], eax
	.loc 2 185 0
	jmp	.L36
.L41:
	.loc 2 187 0
	sub	esp, 8
	push	DWORD PTR [ebp-44]
	push	DWORD PTR [ebp-32]
	call	handle_write
	add	esp, 16
	mov	DWORD PTR [ebp-12], eax
	.loc 2 188 0
	nop
.L36:
	.loc 2 191 0
	mov	eax, DWORD PTR [ebp-36]
	mov	DWORD PTR [eax+20], 5
	.loc 2 192 0
	mov	edx, DWORD PTR [ebp-12]
	mov	eax, DWORD PTR [ebp-36]
	mov	DWORD PTR [eax+24], edx
	.loc 2 194 0
	mov	eax, DWORD PTR [ebp-32]
	mov	eax, DWORD PTR [eax+16]
	sub	esp, 8
	push	DWORD PTR [ebp-36]
	push	eax
	call	msgsend
	add	esp, 16
	movzx	eax, al
	mov	DWORD PTR [ebp-40], eax
	.loc 2 195 0
	cmp	DWORD PTR [ebp-40], 0
	je	.L43
	.loc 2 196 0
	push	OFFSET FLAT:.LC10
	push	196
	push	OFFSET FLAT:.LC1
	push	OFFSET FLAT:__FUNCTION__.1201
	call	__log
	add	esp, 16
	call	abort
.LBE5:
	.loc 2 162 0
	jmp	.L43
	.cfi_endproc
.LFE8:
	.size	main, .-main
	.section	.rodata
	.align 32
	.type	MultiplyDeBruijnBitPosition.1138, @object
	.size	MultiplyDeBruijnBitPosition.1138, 128
MultiplyDeBruijnBitPosition.1138:
	.long	0
	.long	9
	.long	1
	.long	10
	.long	13
	.long	21
	.long	2
	.long	29
	.long	11
	.long	14
	.long	16
	.long	18
	.long	22
	.long	25
	.long	3
	.long	30
	.long	8
	.long	12
	.long	20
	.long	28
	.long	15
	.long	17
	.long	24
	.long	7
	.long	19
	.long	27
	.long	23
	.long	6
	.long	26
	.long	5
	.long	4
	.long	31
	.align 4
	.type	__FUNCTION__.1174, @object
	.size	__FUNCTION__.1174, 12
__FUNCTION__.1174:
	.string	"handle_open"
	.align 4
	.type	__FUNCTION__.1185, @object
	.size	__FUNCTION__.1185, 13
__FUNCTION__.1185:
	.string	"handle_close"
	.align 4
	.type	__FUNCTION__.1198, @object
	.size	__FUNCTION__.1198, 13
__FUNCTION__.1198:
	.string	"handle_write"
	.align 4
	.type	__FUNCTION__.1201, @object
	.size	__FUNCTION__.1201, 5
__FUNCTION__.1201:
	.string	"main"
	.text
.Letext0:
	.file 3 "/Volumes/Kratos/Projects/RastapopoulOS/lib/gcc/i386-pc-elf/6.3.0/include/stdint-gcc.h"
	.file 4 "/Volumes/Kratos/Projects/RastapopoulOS/lib/gcc/i386-pc-elf/6.3.0/include/stddef.h"
	.file 5 "../runtime/runtime.h"
	.file 6 "../../common/port.h"
	.file 7 "vfs.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x6ed
	.value	0x4
	.long	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.long	.LASF63
	.byte	0xc
	.long	.LASF64
	.long	.LASF65
	.long	.Ltext0
	.long	.Letext0-.Ltext0
	.long	.Ldebug_line0
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.long	.LASF0
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.long	.LASF1
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.long	.LASF2
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.long	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.long	.LASF4
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.long	.LASF5
	.uleb128 0x3
	.long	.LASF9
	.byte	0x3
	.byte	0x34
	.long	0x5a
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.long	.LASF6
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.long	.LASF7
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x5
	.long	0x68
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.long	.LASF8
	.uleb128 0x3
	.long	.LASF10
	.byte	0x4
	.byte	0xd8
	.long	0x5a
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.long	.LASF11
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.long	.LASF12
	.uleb128 0x5
	.long	0x8d
	.uleb128 0x6
	.long	0x41
	.long	0xa4
	.uleb128 0x7
	.byte	0
	.uleb128 0x8
	.long	.LASF13
	.byte	0x5
	.byte	0x8
	.long	0x99
	.uleb128 0x8
	.long	.LASF14
	.byte	0x5
	.byte	0x9
	.long	0x99
	.uleb128 0x9
	.long	.LASF23
	.byte	0x1c
	.byte	0x6
	.byte	0x1b
	.long	0x11b
	.uleb128 0xa
	.long	.LASF15
	.byte	0x6
	.byte	0x1c
	.long	0x126
	.byte	0
	.uleb128 0xa
	.long	.LASF16
	.byte	0x6
	.byte	0x1d
	.long	0x4f
	.byte	0x8
	.uleb128 0xa
	.long	.LASF17
	.byte	0x6
	.byte	0x1e
	.long	0x68
	.byte	0xc
	.uleb128 0xa
	.long	.LASF18
	.byte	0x6
	.byte	0x1f
	.long	0x68
	.byte	0x10
	.uleb128 0xa
	.long	.LASF19
	.byte	0x6
	.byte	0x20
	.long	0x74
	.byte	0x14
	.uleb128 0xb
	.string	"len"
	.byte	0x6
	.byte	0x21
	.long	0x74
	.byte	0x18
	.uleb128 0xa
	.long	.LASF20
	.byte	0x6
	.byte	0x22
	.long	0x147
	.byte	0x1c
	.byte	0
	.uleb128 0x5
	.long	0xba
	.uleb128 0xc
	.byte	0x4
	.long	0xba
	.uleb128 0xd
	.byte	0x8
	.byte	0x6
	.byte	0x1c
	.long	0x147
	.uleb128 0xa
	.long	.LASF21
	.byte	0x6
	.byte	0x1c
	.long	0x120
	.byte	0
	.uleb128 0xa
	.long	.LASF22
	.byte	0x6
	.byte	0x1c
	.long	0x120
	.byte	0x4
	.byte	0
	.uleb128 0x6
	.long	0x41
	.long	0x156
	.uleb128 0xe
	.long	0x86
	.byte	0
	.uleb128 0x9
	.long	.LASF24
	.byte	0x8
	.byte	0x7
	.byte	0xb
	.long	0x187
	.uleb128 0xa
	.long	.LASF25
	.byte	0x7
	.byte	0xc
	.long	0x4f
	.byte	0
	.uleb128 0xa
	.long	.LASF26
	.byte	0x7
	.byte	0xd
	.long	0x4f
	.byte	0x4
	.uleb128 0xa
	.long	.LASF27
	.byte	0x7
	.byte	0xe
	.long	0x18c
	.byte	0x8
	.byte	0
	.uleb128 0x5
	.long	0x156
	.uleb128 0x6
	.long	0x8d
	.long	0x19b
	.uleb128 0xe
	.long	0x86
	.byte	0
	.uleb128 0x9
	.long	.LASF28
	.byte	0x8
	.byte	0x7
	.byte	0x11
	.long	0x1bf
	.uleb128 0xb
	.string	"fd"
	.byte	0x7
	.byte	0x12
	.long	0x68
	.byte	0
	.uleb128 0xa
	.long	.LASF29
	.byte	0x7
	.byte	0x13
	.long	0x7b
	.byte	0x4
	.byte	0
	.uleb128 0x5
	.long	0x19b
	.uleb128 0xf
	.long	.LASF30
	.value	0x204
	.byte	0x7
	.byte	0x16
	.long	0x1ea
	.uleb128 0xa
	.long	.LASF31
	.byte	0x7
	.byte	0x17
	.long	0x68
	.byte	0
	.uleb128 0xa
	.long	.LASF20
	.byte	0x7
	.byte	0x18
	.long	0x1ea
	.byte	0x4
	.byte	0
	.uleb128 0x6
	.long	0x8d
	.long	0x1fb
	.uleb128 0x10
	.long	0x86
	.value	0x1ff
	.byte	0
	.uleb128 0x11
	.byte	0x4
	.uleb128 0x9
	.long	.LASF32
	.byte	0x9d
	.byte	0x2
	.byte	0xd
	.long	0x26a
	.uleb128 0xa
	.long	.LASF27
	.byte	0x2
	.byte	0xe
	.long	0x26f
	.byte	0
	.uleb128 0xa
	.long	.LASF25
	.byte	0x2
	.byte	0xf
	.long	0x27f
	.byte	0x64
	.uleb128 0xb
	.string	"uid"
	.byte	0x2
	.byte	0x10
	.long	0x27f
	.byte	0x6c
	.uleb128 0xb
	.string	"gid"
	.byte	0x2
	.byte	0x11
	.long	0x27f
	.byte	0x74
	.uleb128 0xa
	.long	.LASF29
	.byte	0x2
	.byte	0x12
	.long	0x28f
	.byte	0x7c
	.uleb128 0xa
	.long	.LASF33
	.byte	0x2
	.byte	0x13
	.long	0x28f
	.byte	0x88
	.uleb128 0xa
	.long	.LASF34
	.byte	0x2
	.byte	0x14
	.long	0x27f
	.byte	0x94
	.uleb128 0xa
	.long	.LASF35
	.byte	0x2
	.byte	0x15
	.long	0x29f
	.byte	0x9c
	.byte	0
	.uleb128 0x5
	.long	0x1fd
	.uleb128 0x6
	.long	0x8d
	.long	0x27f
	.uleb128 0x12
	.long	0x86
	.byte	0x63
	.byte	0
	.uleb128 0x6
	.long	0x8d
	.long	0x28f
	.uleb128 0x12
	.long	0x86
	.byte	0x7
	.byte	0
	.uleb128 0x6
	.long	0x8d
	.long	0x29f
	.uleb128 0x12
	.long	0x86
	.byte	0xb
	.byte	0
	.uleb128 0x6
	.long	0x8d
	.long	0x2af
	.uleb128 0x12
	.long	0x86
	.byte	0
	.byte	0
	.uleb128 0x13
	.long	.LASF37
	.byte	0x2
	.byte	0x17
	.long	0x2c0
	.uleb128 0x5
	.byte	0x3
	.long	initrd
	.uleb128 0xc
	.byte	0x4
	.long	0x26a
	.uleb128 0x9
	.long	.LASF36
	.byte	0x14
	.byte	0x2
	.byte	0x19
	.long	0x30f
	.uleb128 0xb
	.string	"pid"
	.byte	0x2
	.byte	0x1a
	.long	0x68
	.byte	0
	.uleb128 0xb
	.string	"pos"
	.byte	0x2
	.byte	0x1b
	.long	0x7b
	.byte	0x4
	.uleb128 0xa
	.long	.LASF25
	.byte	0x2
	.byte	0x1c
	.long	0x4f
	.byte	0x8
	.uleb128 0xa
	.long	.LASF20
	.byte	0x2
	.byte	0x1d
	.long	0x30f
	.byte	0xc
	.uleb128 0xa
	.long	.LASF29
	.byte	0x2
	.byte	0x1e
	.long	0x7b
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.byte	0x4
	.long	0x41
	.uleb128 0x6
	.long	0x2c6
	.long	0x325
	.uleb128 0x12
	.long	0x86
	.byte	0x1f
	.byte	0
	.uleb128 0x13
	.long	.LASF38
	.byte	0x2
	.byte	0x20
	.long	0x315
	.uleb128 0x5
	.byte	0x3
	.long	file_descriptors
	.uleb128 0x13
	.long	.LASF39
	.byte	0x2
	.byte	0x21
	.long	0x68
	.uleb128 0x5
	.byte	0x3
	.long	next_fd
	.uleb128 0x14
	.long	.LASF66
	.byte	0x2
	.byte	0x84
	.long	.LFB8
	.long	.LFE8-.LFB8
	.uleb128 0x1
	.byte	0x9c
	.long	0x402
	.uleb128 0x15
	.long	.LASF48
	.long	0x412
	.uleb128 0x5
	.byte	0x3
	.long	__FUNCTION__.1201
	.uleb128 0x16
	.string	"ret"
	.byte	0x2
	.byte	0x88
	.long	0x68
	.uleb128 0x2
	.byte	0x75
	.sleb128 -16
	.uleb128 0x13
	.long	.LASF40
	.byte	0x2
	.byte	0x8e
	.long	0x7b
	.uleb128 0x2
	.byte	0x75
	.sleb128 -20
	.uleb128 0x13
	.long	.LASF41
	.byte	0x2
	.byte	0x8f
	.long	0x30f
	.uleb128 0x2
	.byte	0x75
	.sleb128 -24
	.uleb128 0x13
	.long	.LASF42
	.byte	0x2
	.byte	0x90
	.long	0x1fb
	.uleb128 0x2
	.byte	0x75
	.sleb128 -28
	.uleb128 0x13
	.long	.LASF43
	.byte	0x2
	.byte	0x9a
	.long	0x120
	.uleb128 0x2
	.byte	0x75
	.sleb128 -32
	.uleb128 0x13
	.long	.LASF44
	.byte	0x2
	.byte	0x9d
	.long	0x120
	.uleb128 0x2
	.byte	0x75
	.sleb128 -36
	.uleb128 0x17
	.long	.LBB5
	.long	.LBE5-.LBB5
	.uleb128 0x13
	.long	.LASF45
	.byte	0x2
	.byte	0xa3
	.long	0x74
	.uleb128 0x2
	.byte	0x75
	.sleb128 -48
	.uleb128 0x16
	.string	"ret"
	.byte	0x2
	.byte	0xa4
	.long	0x74
	.uleb128 0x2
	.byte	0x75
	.sleb128 -40
	.uleb128 0x13
	.long	.LASF46
	.byte	0x2
	.byte	0xac
	.long	0x68
	.uleb128 0x2
	.byte	0x75
	.sleb128 -12
	.uleb128 0x16
	.string	"out"
	.byte	0x2
	.byte	0xad
	.long	0x417
	.uleb128 0x2
	.byte	0x75
	.sleb128 -44
	.byte	0
	.byte	0
	.uleb128 0x6
	.long	0x94
	.long	0x412
	.uleb128 0x12
	.long	0x86
	.byte	0x4
	.byte	0
	.uleb128 0x5
	.long	0x402
	.uleb128 0xc
	.byte	0x4
	.long	0x1c4
	.uleb128 0x18
	.long	.LASF49
	.byte	0x2
	.byte	0x7e
	.long	0x68
	.long	.LFB7
	.long	.LFE7-.LFB7
	.uleb128 0x1
	.byte	0x9c
	.long	0x462
	.uleb128 0x19
	.string	"msg"
	.byte	0x2
	.byte	0x7e
	.long	0x462
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x1a
	.long	.LASF47
	.byte	0x2
	.byte	0x7e
	.long	0x417
	.uleb128 0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0x15
	.long	.LASF48
	.long	0x478
	.uleb128 0x5
	.byte	0x3
	.long	__FUNCTION__.1198
	.byte	0
	.uleb128 0xc
	.byte	0x4
	.long	0x11b
	.uleb128 0x6
	.long	0x94
	.long	0x478
	.uleb128 0x12
	.long	0x86
	.byte	0xc
	.byte	0
	.uleb128 0x5
	.long	0x468
	.uleb128 0x18
	.long	.LASF50
	.byte	0x2
	.byte	0x67
	.long	0x68
	.long	.LFB6
	.long	.LFE6-.LFB6
	.uleb128 0x1
	.byte	0x9c
	.long	0x4ea
	.uleb128 0x19
	.string	"msg"
	.byte	0x2
	.byte	0x67
	.long	0x462
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x1a
	.long	.LASF47
	.byte	0x2
	.byte	0x67
	.long	0x417
	.uleb128 0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0x13
	.long	.LASF51
	.byte	0x2
	.byte	0x69
	.long	0x4ea
	.uleb128 0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x16
	.string	"fd"
	.byte	0x2
	.byte	0x6a
	.long	0x4f0
	.uleb128 0x2
	.byte	0x91
	.sleb128 -28
	.uleb128 0x13
	.long	.LASF52
	.byte	0x2
	.byte	0x6c
	.long	0x7b
	.uleb128 0x2
	.byte	0x91
	.sleb128 -32
	.uleb128 0x13
	.long	.LASF29
	.byte	0x2
	.byte	0x72
	.long	0x7b
	.uleb128 0x2
	.byte	0x91
	.sleb128 -20
	.byte	0
	.uleb128 0xc
	.byte	0x4
	.long	0x1bf
	.uleb128 0xc
	.byte	0x4
	.long	0x2c6
	.uleb128 0x18
	.long	.LASF53
	.byte	0x2
	.byte	0x61
	.long	0x68
	.long	.LFB5
	.long	.LFE5-.LFB5
	.uleb128 0x1
	.byte	0x9c
	.long	0x53b
	.uleb128 0x19
	.string	"msg"
	.byte	0x2
	.byte	0x61
	.long	0x462
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x1a
	.long	.LASF47
	.byte	0x2
	.byte	0x61
	.long	0x417
	.uleb128 0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0x15
	.long	.LASF48
	.long	0x478
	.uleb128 0x5
	.byte	0x3
	.long	__FUNCTION__.1185
	.byte	0
	.uleb128 0x18
	.long	.LASF54
	.byte	0x2
	.byte	0x3f
	.long	0x68
	.long	.LFB4
	.long	.LFE4-.LFB4
	.uleb128 0x1
	.byte	0x9c
	.long	0x5d5
	.uleb128 0x19
	.string	"msg"
	.byte	0x2
	.byte	0x3f
	.long	0x462
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x1a
	.long	.LASF47
	.byte	0x2
	.byte	0x3f
	.long	0x417
	.uleb128 0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0x13
	.long	.LASF20
	.byte	0x2
	.byte	0x41
	.long	0x5d5
	.uleb128 0x2
	.byte	0x91
	.sleb128 -24
	.uleb128 0x15
	.long	.LASF48
	.long	0x5eb
	.uleb128 0x5
	.byte	0x3
	.long	__FUNCTION__.1174
	.uleb128 0x16
	.string	"hdr"
	.byte	0x2
	.byte	0x44
	.long	0x2c0
	.uleb128 0x2
	.byte	0x91
	.sleb128 -20
	.uleb128 0x1b
	.long	.Ldebug_ranges0+0
	.uleb128 0x13
	.long	.LASF29
	.byte	0x2
	.byte	0x49
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 -28
	.uleb128 0x17
	.long	.LBB3
	.long	.LBE3-.LBB3
	.uleb128 0x13
	.long	.LASF55
	.byte	0x2
	.byte	0x4c
	.long	0x68
	.uleb128 0x2
	.byte	0x91
	.sleb128 -32
	.uleb128 0x16
	.string	"fd"
	.byte	0x2
	.byte	0x4f
	.long	0x4f0
	.uleb128 0x2
	.byte	0x91
	.sleb128 -36
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0xc
	.byte	0x4
	.long	0x187
	.uleb128 0x6
	.long	0x94
	.long	0x5eb
	.uleb128 0x12
	.long	0x86
	.byte	0xb
	.byte	0
	.uleb128 0x5
	.long	0x5db
	.uleb128 0x1c
	.long	.LASF56
	.byte	0x2
	.byte	0x26
	.long	0x74
	.long	.LFB3
	.long	.LFE3-.LFB3
	.uleb128 0x1
	.byte	0x9c
	.long	0x63f
	.uleb128 0x19
	.string	"in"
	.byte	0x2
	.byte	0x26
	.long	0x63f
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x13
	.long	.LASF29
	.byte	0x2
	.byte	0x28
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 -12
	.uleb128 0x16
	.string	"j"
	.byte	0x2
	.byte	0x29
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 -16
	.uleb128 0x13
	.long	.LASF57
	.byte	0x2
	.byte	0x2a
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 -20
	.byte	0
	.uleb128 0xc
	.byte	0x4
	.long	0x94
	.uleb128 0x1c
	.long	.LASF58
	.byte	0x1
	.byte	0x45
	.long	0x74
	.long	.LFB2
	.long	.LFE2-.LFB2
	.uleb128 0x1
	.byte	0x9c
	.long	0x66b
	.uleb128 0x19
	.string	"v"
	.byte	0x1
	.byte	0x45
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.byte	0
	.uleb128 0x1c
	.long	.LASF59
	.byte	0x1
	.byte	0x31
	.long	0x68
	.long	.LFB1
	.long	.LFE1-.LFB1
	.uleb128 0x1
	.byte	0x9c
	.long	0x6ae
	.uleb128 0x19
	.string	"v"
	.byte	0x1
	.byte	0x31
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x16
	.string	"r"
	.byte	0x1
	.byte	0x33
	.long	0x68
	.uleb128 0x2
	.byte	0x91
	.sleb128 -12
	.uleb128 0x13
	.long	.LASF60
	.byte	0x1
	.byte	0x35
	.long	0x6be
	.uleb128 0x5
	.byte	0x3
	.long	MultiplyDeBruijnBitPosition.1138
	.byte	0
	.uleb128 0x6
	.long	0x6f
	.long	0x6be
	.uleb128 0x12
	.long	0x86
	.byte	0x1f
	.byte	0
	.uleb128 0x5
	.long	0x6ae
	.uleb128 0x1c
	.long	.LASF61
	.byte	0x1
	.byte	0x2c
	.long	0x6e9
	.long	.LFB0
	.long	.LFE0-.LFB0
	.uleb128 0x1
	.byte	0x9c
	.long	0x6e9
	.uleb128 0x19
	.string	"v"
	.byte	0x1
	.byte	0x2c
	.long	0x74
	.uleb128 0x2
	.byte	0x91
	.sleb128 0
	.byte	0
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.long	.LASF62
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0x5
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2116
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x34
	.uleb128 0x19
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2116
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x55
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",@progbits
	.long	0x1c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x4
	.byte	0
	.value	0
	.value	0
	.long	.Ltext0
	.long	.Letext0-.Ltext0
	.long	0
	.long	0
	.section	.debug_ranges,"",@progbits
.Ldebug_ranges0:
	.long	.LBB2-.Ltext0
	.long	.LBE2-.Ltext0
	.long	.LBB4-.Ltext0
	.long	.LBE4-.Ltext0
	.long	0
	.long	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF56:
	.string	"getsize"
.LASF31:
	.string	"result"
.LASF55:
	.string	"fd_index"
.LASF24:
	.string	"vfs_open_data"
.LASF35:
	.string	"typeflag"
.LASF1:
	.string	"short int"
.LASF10:
	.string	"size_t"
.LASF49:
	.string	"handle_write"
.LASF11:
	.string	"sizetype"
.LASF41:
	.string	"heap_start"
.LASF27:
	.string	"filename"
.LASF25:
	.string	"mode"
.LASF23:
	.string	"message"
.LASF13:
	.string	"__START__"
.LASF32:
	.string	"tar_header"
.LASF51:
	.string	"read_data"
.LASF16:
	.string	"checksum"
.LASF50:
	.string	"handle_read"
.LASF42:
	.string	"mmap_ret"
.LASF43:
	.string	"recv_buf"
.LASF18:
	.string	"reply_port"
.LASF37:
	.string	"initrd"
.LASF17:
	.string	"sender"
.LASF3:
	.string	"long long int"
.LASF45:
	.string	"outsiz"
.LASF2:
	.string	"long int"
.LASF63:
	.string	"GNU C99 6.3.0 -masm=intel -mtune=i386 -march=i386 -g -O0 -std=gnu99 -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fno-strict-aliasing"
.LASF48:
	.string	"__FUNCTION__"
.LASF15:
	.string	"node"
.LASF46:
	.string	"out_size"
.LASF65:
	.string	"/Volumes/Kratos/Projects/RastapopoulOS/src/0.13/userland/vfs"
.LASF4:
	.string	"unsigned char"
.LASF30:
	.string	"vfs_result_data"
.LASF0:
	.string	"signed char"
.LASF7:
	.string	"long long unsigned int"
.LASF9:
	.string	"uint32_t"
.LASF58:
	.string	"next_pow2"
.LASF8:
	.string	"unsigned int"
.LASF19:
	.string	"code"
.LASF38:
	.string	"file_descriptors"
.LASF5:
	.string	"short unsigned int"
.LASF12:
	.string	"char"
.LASF44:
	.string	"snd_buf"
.LASF66:
	.string	"main"
.LASF61:
	.string	"is_pow2"
.LASF33:
	.string	"mtime"
.LASF62:
	.string	"_Bool"
.LASF14:
	.string	"__END__"
.LASF36:
	.string	"filedesc"
.LASF34:
	.string	"chksum"
.LASF20:
	.string	"data"
.LASF64:
	.string	"vfs.c"
.LASF6:
	.string	"long unsigned int"
.LASF28:
	.string	"vfs_read_data"
.LASF54:
	.string	"handle_open"
.LASF39:
	.string	"next_fd"
.LASF29:
	.string	"size"
.LASF57:
	.string	"count"
.LASF52:
	.string	"remaining"
.LASF60:
	.string	"MultiplyDeBruijnBitPosition"
.LASF26:
	.string	"perm"
.LASF40:
	.string	"initrd_size"
.LASF22:
	.string	"prev"
.LASF59:
	.string	"log2"
.LASF53:
	.string	"handle_close"
.LASF21:
	.string	"next"
.LASF47:
	.string	"result_buffer"
	.ident	"GCC: (GNU) 6.3.0"
