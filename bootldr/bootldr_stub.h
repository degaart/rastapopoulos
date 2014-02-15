#ifndef _BOOTLDR_STUB_H_
#define _BOOTLDR_STUB_H_

struct REG16 {
	uint16_t ax;
	uint16_t bx;
	uint16_t cx;
	uint16_t dx;
	uint16_t si;
	uint16_t di;
} __attribute__((packed));

#define MAKEWORD(lo,hi) (((lo) & 0xFF) | (((hi) & 0xFF) << 8))
#define MAKEDWORD(lo,hi) (((lo) & 0xFFFF)|(((hi) & 0xFFFF) << 16))

extern void _write_char(uint32_t ch, uint32_t page, uint32_t col);
extern void _halt();
extern void _breakpoint();
extern void _int10(struct REG16* regs);
extern void _int13(struct REG16* regs);
extern uint32_t _check_a20();
extern void _enable_a20();
extern void _memcpyl(void* dst, void* src, uint16_t siz);

#define breakpoint() asm __volatile__("xchgw %%bx,%%bx;"::)


#endif //_BOOTLDR_STUB_H_

