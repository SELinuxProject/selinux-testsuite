#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MEMINFO "/proc/meminfo"
#define MEMINFO_SIZE    2048
#define ERROR printf

/* taken from libhugetlbfs library */

/*
 * Read numeric data from raw and tagged kernel status files.  Used to read
 * /proc and /sys data (without a tag) and from /proc/meminfo (with a tag).
 */
long file_read_ulong(char *file, const char *tag)
{
	int fd;
	char buf[MEMINFO_SIZE];
	int len, readerr;
	char *p, *q;
	long val;

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		ERROR("Couldn't open %s: %s\n", file, strerror(errno));
		return -1;
	}

	len = read(fd, buf, sizeof(buf));
	readerr = errno;
	close(fd);
	if (len < 0) {
		ERROR("Error reading %s: %s\n", file, strerror(readerr));
		return -1;
	}
	if (len == sizeof(buf)) {
		ERROR("%s is too large\n", file);
		return -1;
	}
	buf[len] = '\0';

	/* Search for a tag if provided */
	if (tag) {
		p = strstr(buf, tag);
		if (!p)
			return -1; /* looks like the line we want isn't there */
		p += strlen(tag);
	} else
		p = buf;

	val = strtol(p, &q, 0);
	if (! isspace(*q)) {
		ERROR("Couldn't parse %s value\n", file);
		return -1;
	}

	return val;
}

long getdefaulthugesize(void)
{
	return file_read_ulong(MEMINFO, "Hugepagesize:") * 1024;
}
