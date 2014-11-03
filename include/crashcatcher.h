#ifndef CRASHCATCHER_H
#define CRASHCATCHER_H

void cc_disable(void);

void cc_set_logfile(const char *logfile);
void cc_set_executable(const char *execfile);

#endif /* CRASHCATCHER_H */
