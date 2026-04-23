#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void err_abort(char *msg)
{
	fprintf(stderr, "%s\n", msg);
	fflush(stdout);
	fflush(stderr);
	exit(1);
}
ssize_t readn(int fd, void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while( nleft > 0 ) {
		if( (nread=read(fd, ptr, nleft)) < 0 ) {
			if( errno == EINTR )
				nread = 0; /* doit again */
			else
				return (-1);
		} else if( nread == 0 ) {
			break;		/* EOF */
		}

		nleft -= nread;
		ptr += nread;
	}
	return(n - nleft);
}
ssize_t writen(int fd, void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	char *ptr;
	ptr = vptr;
	nleft = n;
	while( nleft > 0 ) {
		if( (nwritten=write(fd, ptr, nleft)) <= 0 ) {
			if( errno == EINTR )
				nwritten = 0;	/* doit again */
			else
				return(-1);
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return(n);
}

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char c, *ptr;

	ptr = vptr;
	
	for( n=1; n<maxlen; n++ ) {
		again:
		if( (rc=read(fd, &c, 1)) == 1 ) {
			*ptr++ = c;
			if( c=='\n' )
				break;
		} else if( rc == 0 ) {
			if( n == 1 )
				return(0);	/* EOF, no data */
			else
				break;		/* EOF, some data */
		} else {
			if( errno == EINTR )
				goto again;	/* doit again */
			return(-1);		/* Error */
		}
	}
	*ptr = '\0';
	return(n);
}
