#include <string.h>
#include <rasta.h>

void formatv(
    format_callback_t callback, 
    void* callback_params,
    const char* fmt,
    va_list args
) {
    while(*fmt) {
        char num_buffer[16];
        unsigned val;
        char* p;

        switch(*fmt) {
        case '%':
            switch(*(fmt+1)) {
            case 'd':
            case 'u':
                val = va_arg(args, unsigned);

                itoa(num_buffer, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            case 's':
                p = va_arg(args, char*);

                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            case 'X':
            case 'x':
                val = va_arg(args, unsigned);

                itox(num_buffer, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            case 'p':
            case 'P':
                val = va_arg(args, unsigned);

                num_buffer[0] = '0';
                num_buffer[1] = 'x';
                itox(num_buffer + 2, val);
                p = num_buffer;
                while(*p)
                    callback(*(p++), callback_params);

                fmt++;
                break;
            default:
                if(*(fmt+1)) {
                    callback(*(fmt+1), callback_params);
                    fmt++;
                }
            } //switch(*(fmt+1))
            break;
        default:
            callback(*fmt, callback_params);
            break;
        } //switch(*fmt)
        fmt++;
    } // while(fmt)
}

