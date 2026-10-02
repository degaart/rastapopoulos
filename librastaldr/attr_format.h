#pragma once

#ifdef RASTA
#    define ATTR_FORMAT(string_index, first_to_check)                         \
        __attribute__((format(printf, string_index, first_to_check)))
#else
#    define ATTR_FORMAT(string_index, first_to_check)
#endif

