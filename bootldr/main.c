#include "term.h"
#include "util.h"

void main() {
	write_string("Rastapopoulos bootloader\n");
	write_string("Enabling A20 gate\n");
	enable_a20();
	while(1);
}
