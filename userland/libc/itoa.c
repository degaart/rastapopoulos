#include <string.h>

void itoa(char* str, unsigned n) {
    if(n == 0) {
        *str = '0';
        *(str+1) = '\0';
        return;
    } else if(n < 10) {
        *str = '0' + n;
        *(str+1) = '\0';
        return;
    } else {
        char* out = str;
        
        /* max: 65536 */
        unsigned current_divisor = 1000000000;
        int zeroes = 1;
        while(current_divisor) {
            int digit = n / current_divisor;
            if(digit) {
                *(out++) = '0' + digit;
                zeroes = 0;
            } else if(!zeroes)
                *(out++) = '0' + digit;

            
            n %= current_divisor;
            current_divisor /= 10;
        }
        *out = '\0';
    }
}
