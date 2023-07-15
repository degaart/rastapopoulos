#pragma once

#define HALT() while(1) { asm volatile("cli\nhlt\n":::"memory"); }
#define ALIGN(P,A) ((((P) + ((A) - 1)) / (A)) * (A))
#define ROUND(P,A) (((P) / (A)) * (A))

