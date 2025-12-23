#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#include <stat.h>
#include <dirent.h>

int sys_exit(int64_t error_code);
int sys_fork(void);
int sys_wait(int64_t pid, int *store);
int sys_yield(void);
int sys_kill(int64_t pid);
int sys_getpid(void);
int sys_putc(int64_t c);
int sys_pgdir(void);
int sys_gettime(void);
/* FOR LAB6 ONLY */
void sys_lab6_set_priority(uint64_t priority);

int sys_sleep(uint64_t time);

int sys_open(const char *path, uint32_t open_flags);
int sys_close(int fd);
int sys_read(int fd, void *base, size_t len);
int sys_write(int fd, void *base, size_t len);
int sys_seek(int fd, off_t pos, int whence);
int sys_fstat(int fd, struct stat *stat);
int sys_fsync(int fd);
int sys_getcwd(char *buffer, size_t len);
int sys_getdirentry(int fd, struct dirent *dirent);
int sys_dup(int fd1, int fd2);
int sys_exec(const char *name, int argc, const char **argv);

#endif /* !__USER_LIBS_SYSCALL_H__ */

