#ifndef _MEM_H_
#define _MEM_H_

#ifndef _MEM_C_
extern void pokeb(unsigned seg, unsigned ofs, unsigned char val);
extern void poke(unsigned seg, unsigned ofs, unsigned val);
extern unsigned char peekb(unsigned seg, unsigned ofs);
extern unsigned peek(unsigned seg, unsigned ofs);
extern void enable_a20();
#endif

#endif
