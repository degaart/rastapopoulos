#include "runtime.h"
#include "debug.h"

#define invalid_code_path() \
    panic("Invalid code path")

void main()
{
    setname("init");

    /* Programs to run: primes, logger, fib, sleeper */
    int logger_pid = fork();
    if(!logger_pid) {
        exec("logger.elf");
        while(1);
        invalid_code_path();
    }

    int primes_pid = fork();
    if(!primes_pid) {
        exec("primes.elf");
        while(1);
        invalid_code_path();
    }

    int fib_pid = fork();
    if(!fib_pid) {
        exec("fib.elf");
        while(1);
        invalid_code_path();
    }

    int sleeper_pid = fork();
    if(!sleeper_pid) {
        exec("sleeper.elf");
        while(1);
        invalid_code_path();
    }
}

