#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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


static const struct {
    const char *name;
    int signum;
} signals[] = {
    { "Segmentation fault", SIGSEGV },
    { "Illegal instruction", SIGILL },
    { "FPU exception", SIGFPE },
    { "System BUS error", SIGBUS },
    { "Abort", SIGABRT },
    { NULL, 0 }
};


static const struct {
    int code;
    const char *name;
} sigsegv_codes[] = {
#ifndef __FreeBSD__
    { SEGV_MAPERR, "address not mapped to object" },
    { SEGV_ACCERR, "invalid permissions for mapped object" },
#endif
    { 0, NULL }
};

static const struct {
    int code;
    const char *name;
} sigill_codes[] = {
#ifndef __FreeBSD__
    { ILL_ILLOPC, "illegal opcode" },
    { ILL_ILLOPN, "illegal operand" },
    { ILL_ILLADR, "illegal addressing mode" },
    { ILL_ILLTRP, "illegal trap" },
    { ILL_PRVOPC, "privileged opcode" },
    { ILL_PRVREG, "privileged register" },
    { ILL_COPROC, "coprocessor error" },
    { ILL_BADSTK, "internal stack error" },
#endif
    { 0, NULL }
};

static const struct {
    int code;
    const char *name;
} sigfpe_codes[] = {
    { FPE_INTDIV, "integer divide by zero" },
    { FPE_INTOVF, "integer overflow" },
    { FPE_FLTDIV, "floating point divide by zero" },
    { FPE_FLTOVF, "floating point overflow" },
    { FPE_FLTUND, "floating point underflow" },
    { FPE_FLTRES, "floating point inexact result" },
    { FPE_FLTINV, "floating point invalid operation" },
    { FPE_FLTSUB, "subscript out of range" },
    { 0, NULL }
};

static const struct {
    int code;
    const char *name;
} sigbus_codes[] = {
#ifndef __FreeBSD__
    { BUS_ADRALN, "invalid address alignment" },
    { BUS_ADRERR, "non-existent physical address" },
    { BUS_OBJERR, "object specific hardware error" },
#endif
    { 0, NULL }
};

static const struct {
    int code;
    const char *name;
} generic_codes[] = {
    { SI_USER,   "kill() function" },
    { SI_KERNEL, "sent by kernel" },
    { SI_QUEUE,  "sigqueue() function" },
    { SI_TKILL , "tkill() or tgkill() function" },
    { 0, NULL }
};


static void gdb_info(pid_t pid)
{
    char respfile[64];
    char cmd_buf[128];
    FILE *f;
    int fd;

    /* Create a temp file to put gdb commands into */
    strcpy(respfile, "gdb-respfile-XXXXXX");
    if((fd=mkstemp(respfile)) >= 0 && (f=fdopen(fd, "w")) != NULL)
    {
        fprintf(f, "attach %d\n"
                   "shell echo \"\"\n"
                   "shell echo \"* Loaded Libraries\"\n"
                   "info sharedlibrary\n"
                   "shell echo \"\"\n"
                   "shell echo \"* Threads\"\n"
                   "info threads\n"
                   "shell echo \"\"\n"
                   "shell echo \"* FPU Status\"\n"
                   "info float\n"
                   "shell echo \"\"\n"
                   "shell echo \"* Registers\"\n"
                   "info registers\n"
                   "shell echo \"\"\n"
                   "shell echo \"* Backtrace\"\n"
                   "thread apply all backtrace full\n"
                   "detach\n"
                   "quit\n", pid);
        fclose(f);

        /* Run gdb and print process info. */
        snprintf(cmd_buf, sizeof(cmd_buf), "gdb --quiet --batch --command=%s", respfile);
        printf("Executing: %s\n", cmd_buf);
        fflush(stdout);

        system(cmd_buf);
        /* Clean up */
        remove(respfile);
    }
    else
    {
        /* Error creating temp file */
        if(fd >= 0)
        {
            close(fd);
            remove(respfile);
        }
        printf("!!! Could not create gdb command file\n");
    }
    fflush(stdout);
}

static void sys_info(void)
{
#ifdef __unix__
    system("echo \"System: `uname -a`\"");
    putchar('\n');
    fflush(stdout);
#endif
}


static struct crash_info crash_info;

static void crash_handler(const char *logfile)
{
    const char *sigdesc = "Unknown signal";
    const char *codedesc = "unknown code";
    int showlog = 0;
    int i;

    if(fread(&crash_info, sizeof(crash_info), 1, stdin) != 1)
    {
        fprintf(stderr, "!!! Failed to retrieve info from crashed process\n");
        exit(1);
    }
    if(crash_info.version != CRASH_INFO_VERSION)
    {
        fprintf(stderr, "!!! Incompatible crash_info structure (library mismatch)\n");
        exit(1);
    }

    /* Get the signal description */
    for(i = 0;signals[i].name;++i)
    {
        if(signals[i].signum == crash_info.signum)
        {
            sigdesc = signals[i].name;
            break;
        }
    }

    if(crash_info.has_siginfo)
    {
        for(i = 0;generic_codes[i].name;++i)
        {
            if(generic_codes[i].code == crash_info.siginfo.si_code)
            {
                codedesc = generic_codes[i].name;
                break;
            }
        }

        if(!generic_codes[i].name)
        {
            switch(crash_info.signum)
            {
            case SIGSEGV:
                for(i = 0;sigsegv_codes[i].name;++i)
                {
                    if(sigsegv_codes[i].code == crash_info.siginfo.si_code)
                    {
                        codedesc = sigsegv_codes[i].name;
                        break;
                    }
                }
                break;

            case SIGFPE:
                for(i = 0;sigfpe_codes[i].name;++i)
                {
                    if(sigfpe_codes[i].code == crash_info.siginfo.si_code)
                    {
                        codedesc = sigfpe_codes[i].name;
                        break;
                    }
                }
                break;

            case SIGILL:
                for(i = 0;sigill_codes[i].name;++i)
                {
                    if(sigill_codes[i].code == crash_info.siginfo.si_code)
                    {
                        codedesc = sigill_codes[i].name;
                        break;
                    }
                }
                break;

            case SIGBUS:
                for(i = 0;sigbus_codes[i].name;++i)
                {
                    if(sigbus_codes[i].code == crash_info.siginfo.si_code)
                    {
                        codedesc = sigbus_codes[i].name;
                        break;
                    }
                }
                break;
            }
        }
        fprintf(stderr, "%s, %s (signal %i, code %d)\n", sigdesc, codedesc, crash_info.signum, crash_info.siginfo.si_code);
        if(crash_info.signum != SIGABRT)
            fprintf(stderr, "Address: %p\n", crash_info.siginfo.si_addr);
        fputc('\n', stderr);
    }
    else
        fprintf(stderr, "%s (signal %i)\n\n", sigdesc, crash_info.signum);

    if(logfile && *logfile)
    {
        /* Create crash log file and redirect shell output to it */
        if(freopen(logfile, "wa", stdout) != stdout)
            fprintf(stderr, "!!! Could not create %s following signal\n", logfile);
        else
        {
            fprintf(stderr, "Generating %s and killing process %d, please wait...\n", logfile, crash_info.pid);

            puts("*** Fatal Error ***");
            if(!crash_info.has_siginfo)
                printf("%s (signal %i)\n\n", sigdesc, crash_info.signum);
            else
            {
                printf("%s, %s (signal %i, code %d)\n", sigdesc, codedesc, crash_info.signum, crash_info.siginfo.si_code);
                if(crash_info.signum != SIGABRT)
                    printf("Address: %p\n", crash_info.siginfo.si_addr);
                fputc('\n', stdout);
            }
            fflush(stdout);

            showlog = 1;
        }
    }

    sys_info();

    crash_info.buf[sizeof(crash_info.buf)-1] = '\0';
    printf("%s\n", crash_info.buf);
    fflush(stdout);

    if(crash_info.pid > 0)
    {
        gdb_info(crash_info.pid);
        kill(crash_info.pid, SIGKILL);
    }

    if(showlog)
    {
        char buf[512] = {'\0'};
        const char *str;
        int ret = -1;

        if((str=getenv("KDE_FULL_SESSION")) && strcmp(str, "true") == 0)
            snprintf(buf, sizeof(buf), "kdialog --title \"%s - process %d\" --yes-label \"Show log...\" --no-label \"Close\" --yesno \"The application has crashed.\n\nA crash log was written to %s\"", sigdesc, crash_info.pid, logfile);
        else if((str=getenv("GNOME_DESKTOP_SESSION_ID")) && str[0] != '\0')
            snprintf(buf, sizeof(buf), "gxmessage -title \"%s - process %d\" -buttons \"Show log...:0,Close:1\" -center \"The application has crashed.\n\nA crash log was written to %s\"", sigdesc, crash_info.pid, logfile);
        if(buf[0] != '\0')
            ret = system(buf);

        if(ret == -1 || ret == 127 || ret == 0)
        {
            if((str=getenv("KDE_FULL_SESSION")) && strcmp(str, "true") == 0)
                snprintf(buf, sizeof(buf), "kdialog --title \"%s - crash log\" --textbox \"%s\" 800 600", logfile, logfile);
            else if((str=getenv("GNOME_DESKTOP_SESSION_ID")) && str[0] != '\0')
                snprintf(buf, sizeof(buf), "gxmessage -title \"%s - crash log\" -buttons \"Okay:0\" -font monospace -geometry 800x600 -center -file \"%s\"", logfile, logfile);
            else
                snprintf(buf, sizeof(buf), "xmessage -buttons \"Okay:0\" -center -file \"%s\"", logfile);
            system(buf);
        }
    }
}

int main(int argc, char **argv)
{
    cc_disable();

    if(argc < 2 || argc > 3)
    {
        fprintf(stderr, "Invalid number of parameters: %d\n", argc);
        fprintf(stderr, "Usage: %s %s [logfile]\n", argv[0], CRASH_SWITCH);
        exit(1);
    }
    if(strcmp(argv[1], CRASH_SWITCH) != 0)
    {
        fprintf(stderr, "Invalid parameter: %s\n", argv[1]);
        fprintf(stderr, "Usage: %s %s [logfile]\n", argv[0], CRASH_SWITCH);
        exit(1);
    }

    crash_handler((argc==3) ? argv[2] : NULL);
    return 0;
}
