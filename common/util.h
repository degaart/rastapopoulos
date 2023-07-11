#pragma once

#define HALT() while(1) { asm volatile("cli\nhlt\n":::"memory"); }
