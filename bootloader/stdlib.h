#pragma once

#ifdef RASTA

#include <stddef.h>

void *malloc( size_t size );
void free( void *ptr );

#else
#include_next <stdlib.h>
#endif

