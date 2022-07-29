/* ufind: simple find(1) replacement with destination uniqueness checks
 *
 * SPDX-License-Identifier: Apache-2.0
 * (c) 2022, Konstantin Demin
 *
 * Rough alternative (but slow):
 *  find "$@" -follow -type f -print0 \
 *  | xargs -0 -r -n 128 stat -L --printf='%d:%i|%n\0' \
 *  | sort -z -u -t '|' -k1,1 \
 *  | cut -z -d '|' -f 2
 */

#define _GNU_SOURCE

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "uhash.h"

static void usage(void)
{
	fprintf(stderr,
		"Usage: ufind [-z] <path> [..<path>]\n"
		"  -z  - separate entries with \\0 instead of \\n\n"
	);
}

static void prepare_internals(void);
static void process_arg(const char * arg);

static char entry_separator = '\n';

int main(int argc, char * argv[])
{
	if (argc < 2) {
		usage();
		return 0;
	}

	// skip 1st argument
	argc--; argv++;

	if (strcmp(argv[0], "-z") == 0) {
		entry_separator = 0;
		// skip argument
		argc--; argv++;
	}

	prepare_internals();

	for (int i = 0; i < argc; i++) {
		process_arg(argv[i]);
	}

	return 0;
}

typedef struct { char path[4096]; } path;

/* "visited" entries */
UHASH_DEFINE__TYPE0(uh_ino, ino_t);
UHASH_DEFINE__TYPE2(uh_dev_ino, dev_t, uh_ino);

UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(dev_t);
UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(ino_t);

static uh_dev_ino filenames;
static uh_dev_ino visited_dirs;

static void dump_error(int error_num, const char * where);
static void dump_path_error(int error_num, const char * where, const char * name);

static void process_file(dev_t dev, ino_t ino, const char * name);
static void process_dir(dev_t dev, ino_t ino, const char * name);

static int handle_file_type(mode_t type, const char * arg);
static int resolve_fd(int fd, char * buffer, size_t buffer_size);

static void process_arg(const char * name)
{
	int f_fd = open(name, O_RDONLY | O_PATH);
	if (f_fd < 0) {
		dump_path_error(errno, "process_arg:open(2)", name);
		return;
	}

	struct stat f_stat;
	memset(&f_stat, 0, sizeof(f_stat));

	if (fstat(f_fd, &f_stat) < 0) {
		dump_path_error(errno, "process_arg:fstat(2)", name);
		goto process_arg__close;
	}

	if (handle_file_type(f_stat.st_mode, name) == 0) {
		goto process_arg__close;
	}

	char * tname = calloc(1, sizeof(path));
	if (resolve_fd(f_fd, tname, sizeof(path)) == 0) {
		goto process_arg__free;
	}

	close(f_fd); f_fd = -1;

	switch (f_stat.st_mode & S_IFMT) {
	case S_IFREG: process_file(f_stat.st_dev, f_stat.st_ino, tname); break;
	case S_IFDIR: process_dir(f_stat.st_dev, f_stat.st_ino, tname); break;
	}

process_arg__free:

	if (tname != NULL) {
		free(tname);
	}

process_arg__close:

	if (f_fd >= 0) {
		close(f_fd);
	}

	return;
}

static void process_file(dev_t dev, ino_t ino, const char * name)
{
	uhash_idx_t i_dev = UHASH_CALL(uh_dev_ino, search, &filenames, dev);
	if (i_dev == 0) {
		uh_ino empty;
		memset(&empty, 0, sizeof(empty));
		i_dev = UHASH_CALL(uh_dev_ino, insert, &filenames, dev, &empty);
	}

	uh_ino * h_ino = (uh_ino *) UHASH_CALL(uh_dev_ino, value, &filenames, i_dev);
	uhash_idx_t i_ino = UHASH_CALL(uh_ino, search, h_ino, ino);
	if (i_ino != 0) {
		return;
	}

	UHASH_CALL(uh_ino, insert, h_ino, ino);

	fputs(name, stdout);
	fputc(entry_separator, stdout);
}

static inline int filter_out_dots(const struct dirent * entry)
{
	/*
	if (strcmp(entry->d_name, ".") == 0)
		return 0;
	if (strcmp(entry->d_name, "..") == 0)
		return 0;
	return 1;
	*/

	if (entry->d_name[0] != '.')
		return 1;
	if (entry->d_name[1] == 0)
		return 0;
	if (entry->d_name[1] != '.')
		return 1;
	if (entry->d_name[2] == 0)
		return 0;

	return 1;
}

static void process_dir(dev_t dev, ino_t ino, const char * name)
{
	uhash_idx_t i_dev = UHASH_CALL(uh_dev_ino, search, &visited_dirs, dev);
	if (i_dev == 0) {
		uh_ino empty;
		memset(&empty, 0, sizeof(empty));
		i_dev = UHASH_CALL(uh_dev_ino, insert, &visited_dirs, dev, &empty);
	}

	uh_ino * h_ino = (uh_ino *) UHASH_CALL(uh_dev_ino, value, &visited_dirs, i_dev);
	uhash_idx_t i_ino = UHASH_CALL(uh_ino, search, h_ino, ino);
	if (i_ino != 0) {
		return;
	}

	UHASH_CALL(uh_ino, insert, h_ino, ino);

	DIR * d = opendir(name);
	if (d == NULL) {
		dump_path_error(errno, "process_dir:opendir(3)", name);
		return;
	}
	
	char * tname = malloc(sizeof(path));

	struct dirent * dent;
	while ((dent = readdir(d)) != NULL) {
		if (filter_out_dots(dent) == 0) {
			continue;
		}

		memset(tname, 0, sizeof(path));
		snprintf(tname, sizeof(path) - 1, "%s/%s", name, dent->d_name);

		switch (dent->d_type) {
		case DT_REG:
			process_file(dev, dent->d_ino, tname);
			break;
		case DT_DIR:
			process_dir(dev, dent->d_ino, tname);
			break;
		case DT_LNK:
			process_arg(tname);
			break;
		}
	}

	free(tname);

	closedir(d);
}

static int uh_ino__ctor(uh_ino * hash)
{
	UHASH_CALL(uh_ino, init, hash);
	UHASH_SET_DEFAULT_KEY_COMPARATOR(hash, ino_t);
	return 0;
}

static int uh_ino__dtor(uh_ino * hash)
{
	UHASH_CALL(uh_ino, free, hash);
	return 0;
}

static void prepare_internals(void)
{
	UHASH_CALL(uh_dev_ino, init, &filenames);
	UHASH_SET_DEFAULT_KEY_COMPARATOR(&filenames, dev_t);
	UHASH_SET_VALUE_HANDLERS(&filenames, uh_ino__ctor, uh_ino__dtor);

	UHASH_CALL(uh_dev_ino, init, &visited_dirs);
	UHASH_SET_DEFAULT_KEY_COMPARATOR(&visited_dirs, dev_t);
	UHASH_SET_VALUE_HANDLERS(&visited_dirs, uh_ino__ctor, uh_ino__dtor);
}

static int handle_file_type(mode_t type, const char * arg)
{
	const char * e_type = NULL;
	switch (type & S_IFMT) {
	case S_IFREG:  break;
	case S_IFDIR:  break;
	case S_IFBLK:  e_type = "block device"; break;
	case S_IFCHR:  e_type = "character device"; break;
	case S_IFIFO:  e_type = "FIFO"; break;
	case S_IFLNK:  e_type = "symbolic link"; break;
	case S_IFSOCK: e_type = "socket"; break;
	default:       e_type = "unknown entry type"; break;
	}

	if (e_type != NULL) {
		fprintf(stderr, "can't handle <%s>, skipping %s\n", e_type, arg);
		return 0;
	}

	return 1;
}

static int resolve_fd(int fd, char * buffer, size_t buffer_size)
{
	static char proc_link[40];

	memset(proc_link, 0, sizeof(proc_link));
	snprintf(proc_link, sizeof(proc_link) - 1, "/proc/self/fd/%d", fd);
	ssize_t result = readlink(proc_link, buffer, buffer_size);
	if (result < 0) {
		dump_error(errno, "resolve_fd:readlink(2)");
		return 0;
	}
	return 1;
}

static void dump_error(int error_num, const char * where)
{
	char        * e_str = NULL;
	static char   e_buf[8192];

	memset(&e_buf, 0, sizeof(e_buf));
	e_str = strerror_r(error_num, e_buf, sizeof(e_buf) - 1);
	fprintf(stderr, "%s error %d: %s\n", where, error_num, e_str);
}

static void dump_path_error(int error_num, const char * where, const char * name)
{
	char        * e_str = NULL;
	static char   e_buf[8192];

	memset(&e_buf, 0, sizeof(e_buf));
	e_str = strerror_r(error_num, e_buf, sizeof(e_buf) - 1);
	fprintf(stderr, "%s path '%s' error %d: %s\n", where, name, error_num, e_str);
}