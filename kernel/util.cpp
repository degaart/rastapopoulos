#include "util.h"

uint32_t Util::_rand_seed = 1;

void Util::srand(uint32_t seed) {
    _rand_seed = seed;
}

/* Shamelessly stolen from unix v7 */
uint32_t Util::rand() {
    return(((_rand_seed = _rand_seed * 1103515245 + 12345)>>16) & 077777);
}

