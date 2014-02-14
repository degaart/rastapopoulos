#include <stdio.h>

#define SPT 18
#define HEADS 2

struct CHS {
	unsigned c, h, s;
};

struct CHS lsect2chs(unsigned lsect) {
	struct CHS chs;
	
	unsigned temp = (lsect-1)/SPT;
	chs.s = ((lsect-1) % SPT)+1;
	chs.h = temp % HEADS;
	chs.c = temp / HEADS;
	return(chs);
}

int main() {
	struct CHS chs;
	for(int i=1; i<=2880; i++) {
		chs = lsect2chs(i);
		printf(
			"{ lsect: %u, c: %u, h: %u, s: %u }\n",
			i,
			chs.c,
			chs.h,
			chs.s
		);
	}
	return(0);
}

