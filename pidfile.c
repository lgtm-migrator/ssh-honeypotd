#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "pidfile.h"

#define BUF_SIZE 64

static int lock_file(int fd, int type, short int whence, off_t start, off_t len)
{
	struct flock fl;

	fl.l_type   = type;
	fl.l_whence = whence;
	fl.l_start  = start;
	fl.l_len    = len;

#ifdef F_OFD_SETLK
	return fcntl(fd, F_OFD_SETLK, &fl);
#else
	return fcntl(fd, F_SETLK, &fl);
#endif
}

int create_pid_file(const char* path)
{
	int flags;
	int fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (-1 == fd) {
		return -1;
	}

	flags = fcntl(fd, F_GETFD);
	if (flags != -1) {
		flags |= FD_CLOEXEC;
		fcntl(fd, F_SETFD, flags);
	}

	if (lock_file(fd, F_WRLCK, SEEK_SET, 0, 0) == -1) {
		if (errno  == EAGAIN || errno == EACCES) {
			/* PID file locked - another instance is running */
			close(fd);
			return -2;
		}

		close(fd);
		return -1;
	}

	if (ftruncate(fd, 0) == -1) {
		close(fd);
		return -1;
	}

#ifndef F_OFD_SETLK
	lock_file(fd, F_UNLCK, SEEK_SET, 0, 0);
#endif

	return fd;
}

int write_pid(int fd)
{
	char buf[BUF_SIZE];
	snprintf(buf, BUF_SIZE, "%ld\n", (long int)getpid());

#ifndef F_OFD_SETLK
	if (lock_file(fd, F_WRLCK, SEEK_SET, 0, 0)) {
		return -1;
	}
#endif

	if (write(fd, buf, strlen(buf)) != strlen(buf)) {
		return -1;
	}

	return fsync(fd);
}
