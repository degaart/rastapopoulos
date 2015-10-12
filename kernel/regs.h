#ifndef _REGS_H_
#define _REGS_H_

#ifndef __APPLE__
#define write_cr3(x)    asm volatile("mov %0, %%cr3" :: "r"(x))
#define write_cr2(x)    asm volatile("mov %0, %%cr2" :: "r"(x))
#define write_cr1(x)    asm volatile("mov %0, %%cr1" :: "r"(x))
#define write_cr0(x)    asm volatile("mov %0, %%cr0" :: "r"(x))

#define read_cr0(x)     asm volatile("mov %%cr0, %0" : "=r"(x))
#define read_cr1(x)     asm volatile("mov %%cr1, %0" : "=r"(x))
#define read_cr2(x)     asm volatile("mov %%cr2, %0" : "=r"(x))
#define read_cr3(x)     asm volatile("mov %%cr3, %0" : "=r"(x))

#define read_esp(x)     asm volatile("mov %%esp, %0" : "=r"(x))
#else
#define write_cr3(x)
#define write_cr2(x)
#define write_cr1(x)
#define write_cr0(x)

#define read_cr0(x)
#define read_cr1(x)
#define read_cr2(x)
#define read_cr3(x)

#define read_esp(x)
#endif

#define CR0_PG  (1 << 31)
#define CR0_CD  (1 << 30)
#define CR0_NW  (1 << 29)
#define CR0_AM  (1 << 18)
#define CR0_WP  (1 << 16)
#define CR0_NE  (1 << 5)
#define CR0_ET  (1 << 4)
#define CR0_TS  (1 << 3)
#define CR0_EM  (1 << 2)
#define CR0_MP  (1 << 1)
#define CR0_PE  (1)

#define CR3_PCD (1 << 4)
#define CR3_PWT (1 << 3)

#define CR4_VME (1)
#define CR4_PVI (1 << 1)
#define CR4_TSD (1 << 2)
#define CR4_DE  (1 << 3)
#define CR4_PSE (1 << 4)
#define CR4_PAE (1 << 5)
#define CR4_MCE (1 << 6)
#define CR4_PGE (1 << 7)
#define CR4_PCE (1 << 8)
#define CR4_PSFXSR      (1 << 9)
#define CR4_OSXMMEXCPT  (1 << 10)
#define CR4_VMXE        (1 << 13)
#define CR4_SMXE        (1 << 14)
#define CR4_FSGSBASE    (1 << 16)
#define CR4_PCIDE       (1 << 17)
#define CR4_OSXSAVE     (1 << 18)
#define CR4_SMEP        (1 << 20)

#endif
