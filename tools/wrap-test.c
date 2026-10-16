#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void print_help(void)
{
    puts("Usage: wrap-test -r=N [-t=N] [--] command [arguments...]\n"
         "  -r=N    expected exit status (0-255); required\n"
         "  -t=N    timeout in seconds; 0 or omitted means no timeout\n"
         "  --help  show this help page");
}

static int parse_number(const char* value, unsigned long maximum,
                        unsigned long* result)
{
    unsigned long number = 0;

    if (*value == '\0') {
        return -1;
    }
    for (; *value != '\0'; value++) {
        unsigned int digit;

        if (*value < '0' || *value > '9') {
            return -1;
        }
        digit = (unsigned int)(*value - '0');
        if (number > (maximum - digit) / 10) {
            return -1;
        }
        number = number * 10 + digit;
    }
    *result = number;
    return 0;
}

static int time_expired(const struct timespec* start, int timeout)
{
    struct timespec now;
    time_t seconds;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        return -1;
    }
    seconds = now.tv_sec - start->tv_sec;
    return seconds > timeout ||
           (seconds == timeout && now.tv_nsec >= start->tv_nsec);
}

static int wait_for_command(pid_t child, int timeout, int* status)
{
    struct timespec start;
    struct timespec pause = {0, 10000000};

    if (timeout > 0 && clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        return -1;
    }
    for (;;) {
        pid_t result = waitpid(child, status, timeout > 0 ? WNOHANG : 0);

        if (result == child) {
            return 0;
        }
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (time_expired(&start, timeout) != 0) {
            return 1;
        }
        nanosleep(&pause, NULL);
    }
}

static void stop_command(pid_t child, int has_group)
{
    int child_status;

    if (has_group) {
        kill(-child, SIGKILL);
    }
    kill(child, SIGKILL);
    while (waitpid(child, &child_status, 0) == -1 && errno == EINTR) {
    }
}

static int set_terminal_group(pid_t group)
{
    sigset_t blocked_signals;
    sigset_t old_signals;
    int result;
    int saved_errno;

    sigemptyset(&blocked_signals);
    sigaddset(&blocked_signals, SIGTTOU);
    if (sigprocmask(SIG_BLOCK, &blocked_signals, &old_signals) == -1) {
        return -1;
    }
    result = tcsetpgrp(STDIN_FILENO, group);
    saved_errno = errno;
    sigprocmask(SIG_SETMASK, &old_signals, NULL);
    errno = saved_errno;
    return result;
}

int main(int argc, char* argv[])
{
    int expected_status = -1;
    int timeout = 0;
    int option_index = 1;
    int error_pipe[2];
    int child_status;
    int wait_result;
    pid_t child;
    unsigned long parsed_number;
    ssize_t error_size;
    int launch_error;
    int terminal_transferred = 0;

    while (option_index < argc) {
        const char* argument = argv[option_index];

        if (strcmp(argument, "--help") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argument, "--") == 0) {
            option_index++;
            break;
        }
        if (strncmp(argument, "-r=", 3) == 0) {
            if (expected_status != -1 ||
                parse_number(argument + 3, 255, &parsed_number) == -1) {
                fprintf(stderr, "wrap-test: invalid -r value\n");
                return 1;
            }
            expected_status = (int)parsed_number;
        } else if (strncmp(argument, "-t=", 3) == 0) {
            if (parse_number(argument + 3, INT_MAX, &parsed_number) == -1) {
                fprintf(stderr, "wrap-test: invalid -t value\n");
                return 1;
            }
            timeout = (int)parsed_number;
        } else if (argument[0] == '-') {
            fprintf(stderr, "wrap-test: unknown option: %s\n", argument);
            return 1;
        } else {
            break;
        }
        option_index++;
    }
    if (expected_status == -1 || option_index == argc) {
        fprintf(stderr, "wrap-test: expected -r=N and a command\n");
        return 1;
    }
    if (pipe2(error_pipe, O_CLOEXEC) == -1) {
        perror("wrap-test: pipe2");
        return 1;
    }
    child = fork();
    if (child == -1) {
        perror("wrap-test: fork");
        close(error_pipe[0]);
        close(error_pipe[1]);
        return 1;
    }
    if (child == 0) {
        int child_error;

        close(error_pipe[0]);
        if (timeout > 0 && setpgid(0, 0) == -1) {
            child_error = errno;
        } else {
            execvp(argv[option_index], &argv[option_index]);
            child_error = errno;
        }
        while (write(error_pipe[1], &child_error, sizeof(child_error)) == -1 &&
               errno == EINTR) {
        }
        _exit(127);
    }
    close(error_pipe[1]);
    if (timeout > 0) {
        setpgid(child, child);
        if (tcgetpgrp(STDIN_FILENO) == getpgrp() &&
            set_terminal_group(child) == 0) {
            terminal_transferred = 1;
            kill(-child, SIGCONT);
        }
    }
    wait_result = wait_for_command(child, timeout, &child_status);
    if (wait_result != 0) {
        stop_command(child, timeout > 0);
        if (terminal_transferred) {
            set_terminal_group(getpgrp());
        }
        close(error_pipe[0]);
        if (wait_result == -1) {
            perror("wrap-test: wait");
        }
        return 1;
    }
    if (terminal_transferred) {
        set_terminal_group(getpgrp());
    }
    do {
        error_size = read(error_pipe[0], &launch_error, sizeof(launch_error));
    } while (error_size == -1 && errno == EINTR);
    close(error_pipe[0]);
    if (error_size == sizeof(launch_error)) {
        fprintf(stderr, "wrap-test: cannot run %s: %s\n", argv[option_index],
                strerror(launch_error));
        return 1;
    }
    if (error_size != 0) {
        fprintf(stderr, "wrap-test: could not read command launch status\n");
        return 1;
    }
    return WIFEXITED(child_status) &&
                   WEXITSTATUS(child_status) == expected_status
               ? 0
               : 1;
}
