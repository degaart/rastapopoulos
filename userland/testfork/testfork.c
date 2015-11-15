/*
    Program to test fork();

    #include <unistd.h>
    pid_t fork(void);

    Upon successful completion, fork() returns a value of 0 to the child process and returns
    the process ID of the child process to the parent process.
    Otherwise, a value of -1 is returned to the parent process, no child process is created,
    and the global variable errno is set to indicate the error.
*/
#include <stdio.h>
#include <stdlib.h>
#include <rasta.h>
#include <unistd.h>

int main() {
    rs_trace("Forking...");
    pid_t pid = fork();
    if(pid == -1) {
        rs_trace("fork() failed. Bummer");
        exit(1);
    } else if(pid == 0) {
        rs_trace("Inside child process");
    } else {
        rs_trace("Child PID: %d", pid);
    }
    return 0;    
}

