#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main()
{
    const char terminators[] = "%spudxX";
    const char* modifiers[] = {"", "z", "l"};
    const char* paddings[] = {"", "2", "02", "10", "11", "010", "011"};

    for(size_t i = 0; i < strlen(terminators); i++) {
        bool mod, pad;
        switch(terminators[i]) {
        case '%': /* no modifiers, no paddings */
            mod = pad = false;
            break;
        case 's': /* no modifiers */
            mod = false;
            pad = true;
            break;
        case 'p': /* no modifiers, no paddings */
            mod = false;
            pad = false;
            break;
        default: /* modifiers and paddings */
            mod = pad = true;
            break;
        }

        for(size_t j = 0; j < sizeof(modifiers) / sizeof(modifiers[0]); j++) {
            for(size_t k = 0; k < sizeof(paddings) / sizeof(paddings[0]); k++) {
                char fmt[128];
                strlcpy(fmt, "%", sizeof(fmt));
                if(pad) {
                    snprintf(fmt + strlen(fmt), sizeof(fmt) - strlen(fmt), "%s",
                             paddings[k]);
                }
                if(mod) {
                    snprintf(fmt + strlen(fmt), sizeof(fmt) - strlen(fmt), "%s",
                             modifiers[j]);
                }
                snprintf(fmt + strlen(fmt), sizeof(fmt) - strlen(fmt), "%c",
                         terminators[i]);

                char result[64];
                char args[64] = {0};
                switch(terminators[i]) {
                case '%':
                    snprintf(result, sizeof(result), fmt);
                    break;
                case 's':
                    snprintf(result, sizeof(result), fmt, "aBCd");
                    strlcpy(args, ", \"aBCd\"", sizeof(args));
                    break;
                case 'p':
                    snprintf(result, sizeof(result), fmt, (void*)0xDEADBEEF);
                    strlcpy(args, ", (void*)0xDEADBEEF", sizeof(args));
                    break;
                case 'u':
                case 'd':
                    snprintf(result, sizeof(result), fmt, 1234567890);
                    strlcpy(args, ", 123456780", sizeof(args));
                    break;
                case 'x':
                case 'X':
                    snprintf(result, sizeof(result), fmt, 0x12345678);
                    strlcpy(args, ", 0x12345678", sizeof(args));
                    break;
                }

                printf("TEST(\"%s\", \"%s\"%s);\n", result, fmt, args);
            }
        }
    }

    char result[64];
    snprintf(result, sizeof(result), "%-11s", "aBCd");
    printf("TEST(\"%s\", \"%%-11s\", \"aBCd\");\n", result);

    snprintf(result, sizeof(result), "%-10s", "aBCd");
    printf("TEST(\"%s\", \"%%-10s\", \"aBCd\");\n", result);

    snprintf(result, sizeof(result), "%-2s", "aBCd");
    printf("TEST(\"%s\", \"%%-2s\", \"aBCd\");\n", result);
    return 0;
}
