#include <stddef.h>
#include <sys/types.h>

ssize_t writen(int fd, void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);
ssize_t readline_util(int fd, void *vptr, size_t maxlen);

void err_abort(char *msg);

