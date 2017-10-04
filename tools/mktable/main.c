#include <stdio.h>
#include <math.h>

int main()
{
    /* create the sin(arccos(x)) table. */
    printf("static int32_t sin_acos[1024] = {\n"
           "    ");
    for(int i = 0; i < 1024; i++) {
        if(i > 0 && (i % 10) == 0)
            printf("\n    ");

        int val = sin(acos((float)i/1024))*0x10000L;
        printf("0x%05x, ", val);
    }
    printf("\n};\n");
    return 0;
}


