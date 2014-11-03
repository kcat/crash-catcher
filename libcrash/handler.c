#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/ucontext.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef __linux__
#include <sys/prctl.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#endif

#include "main.h"

#include "crashcatcher.h"


#define UNUSED(x) UNUSED_##x __attribute__((unused))

static const char fatal_err[] = "\n\n*** Fatal Error ***\n";
static const char pipe_err[] = "!!! Failed to create pipe\n";
static const char fork_err[] = "!!! Failed to fork debug process\n";
static const char exec_err[] = "!!! Failed to exec debug process\n";

static char altstack[SIGSTKSZ];

static char exec_name[PATH_MAX] = CRASHCATCH_NAME;
static char log_name[PATH_MAX] = "/tmp/libcrash-log.txt";

static struct sigaction old_sigsegv_action;
static struct sigaction old_sigill_action;
static struct sigaction old_sigfpe_action;
static struct sigaction old_sigbus_action;
static struct sigaction old_sigabrt_action;

struct crash_info crash_info;


static size_t safe_write(int fd, const void *buf, size_t len)
{
    size_t ret = 0;
    while(ret < len)
    {
        ssize_t rem;
        if((rem=write(fd, (const char*)buf+ret, len-ret)) == -1)
        {
            if(errno == EINTR)
                continue;
            break;
        }
        ret += rem;
    }
    return ret;
}

static void crash_catcher(int signum, siginfo_t *siginfo, void *UNUSED(context))
{
    pid_t dbg_pid;
    int fd[2];

    /* Make sure the effective uid is the real uid */
    if(getuid() != geteuid())
    {
        raise(signum);
        return;
    }

    safe_write(STDERR_FILENO, fatal_err, sizeof(fatal_err)-1);
    if(pipe(fd) == -1)
    {
        safe_write(STDERR_FILENO, pipe_err, sizeof(pipe_err)-1);
        raise(signum);
        return;
    }

    crash_info.signum = signum;
    crash_info.pid = getpid();
    crash_info.has_siginfo = !!siginfo;
    if(siginfo)
        crash_info.siginfo = *siginfo;

    /* Fork off to start a crash handler */
    switch((dbg_pid=fork()))
    {
        /* Error */
        case -1:
            safe_write(STDERR_FILENO, fork_err, sizeof(fork_err)-1);
            raise(signum);
            return;

        case 0:
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            close(fd[1]);

            unsetenv("LD_PRELOAD");
            execlp(exec_name, exec_name, CRASH_SWITCH, log_name, NULL);

            safe_write(STDERR_FILENO, exec_err, sizeof(exec_err)-1);
            _exit(1);

        default:
#ifdef __linux__
            prctl(PR_SET_PTRACER, dbg_pid, 0, 0, 0);
#endif
            safe_write(fd[1], &crash_info, sizeof(crash_info));
            close(fd[0]);
            close(fd[1]);

            /* Wait; we'll be killed when gdb is done */
            do {
                int status;
                if(waitpid(dbg_pid, &status, 0) == dbg_pid &&
                   (WIFEXITED(status) || WIFSIGNALED(status)))
                {
                    /* The debug process died before it could kill us */
                    raise(signum);
                    break;
                }
            } while(1);
    }
}

static void install_handlers(void)
{
    struct sigaction sa;
    stack_t altss;

    /* Set an alternate signal stack so SIGSEGVs caused by stack overflows
     * still run */
    altss.ss_sp = altstack;
    altss.ss_flags = 0;
    altss.ss_size = sizeof(altstack);
    sigaltstack(&altss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_catcher;
    sa.sa_flags = SA_RESETHAND | SA_NODEFER | SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, &old_sigsegv_action);
    sigaction(SIGILL, &sa, &old_sigill_action);
    sigaction(SIGFPE, &sa, &old_sigfpe_action);
    sigaction(SIGBUS, &sa, &old_sigbus_action);
    sigaction(SIGABRT, &sa, &old_sigabrt_action);
}

static __attribute__((constructor)) void _installer_constructor()
{
    printf("Installing crash handlers...\n");
    install_handlers();
}

void cc_set_logfile(const char *logfile)
{
    if(!logfile) logfile = "";
    snprintf(log_name, sizeof(log_name), "%s", logfile);
}

void cc_set_executable(const char *execfile)
{
    if(!execfile) execfile = CRASHCATCH_NAME;
    snprintf(exec_name, sizeof(exec_name), "%s", execfile);
}

void cc_disable(void)
{
    sigaction(SIGSEGV, &old_sigsegv_action, NULL);
    sigaction(SIGILL, &old_sigill_action, NULL);
    sigaction(SIGFPE, &old_sigfpe_action, NULL);
    sigaction(SIGBUS, &old_sigbus_action, NULL);
    sigaction(SIGABRT, &old_sigabrt_action, NULL);
}
