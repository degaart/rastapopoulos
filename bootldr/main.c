#include "term.h"
#include "util.h"
#include "io.h"
#include "debug.h"

void main() {
    TRACE("*** Rastapopoulos bootloader ***");
	TRACE("Enabling A20 gate");
	enable_a20();
    halt();
}
