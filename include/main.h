#ifndef CC_MAIN_H
#define CC_MAIN_H

#include <sys/types.h>
#include <signal.h>

struct crash_info {
    int signum;
    pid_t pid;
    int has_siginfo;
    siginfo_t siginfo;
    char buf[1024];
} __attribute__((packed));

#define CRASH_SWITCH "--cc-handle-crash"

#endif /* CC_MAIN_H */
