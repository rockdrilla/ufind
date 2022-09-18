/* ufind: simple find(1) replacement with target uniqueness checks
 *
 * SPDX-License-Identifier: Apache-2.0
 * (c) 2022, Konstantin Demin
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

#include "include/uhash/uhash.h"

#define UFIND_OPTS "hqsz"

static void usage(int retcode)
{
	fputs(
	"ufind 0.3.0\n"
	"Usage: ufind [-" UFIND_OPTS "] <path> [..<path>]\n"
	" -h  - help (show this message)\n"
	" -q  - quiet (don't print information messages in stderr)\n"
	" -s  - silent (don't print error messages in stderr)\n"
	" -z  - zero-separator (separate entries with \\0 instead of \\n)\n"
	, stderr);

	exit(retcode);
}

static struct {
	int posix
	,	Quiet
	,	Silent
	,	Zero_separator
	;
} opt;

static char entry_separator = '\n';

static void init_opts(void);
static void parse_flags(int argc, char * argv[]);

static void prepare_internals(void);
static void process_arg(const char * arg);

int main(int argc, char * argv[])
{
	if (argc < 2) {
		usage(0);
		return 0;
	}

	parse_flags(argc, argv);
	if (optind >= argc) usage(EINVAL);
	if (opt.Zero_separator) entry_separator = 0;

	prepare_internals();

	for (int i = optind; i < argc; i++) {
		process_arg(argv[i]);
	}

	return 0;
}

static void init_opts(void)
{
	memset(&opt, 0, sizeof(opt));

	if (getenv("POSIXLY_CORRECT")) opt.posix = 1;
}

static void parse_flags(int argc, char * argv[])
{
	int o;
	while ((o = getopt(argc, argv, UFIND_OPTS)) != -1) {
		switch (o) {
		case 'h':
			usage(0);
			break;
		case 'q':
			if (opt.posix && opt.Quiet) break;
			opt.Quiet = 1;
			continue;
		case 's':
			if (opt.posix && opt.Silent) break;
			opt.Silent = 1;
			continue;
		case 'z':
			if (opt.posix && opt.Zero_separator) break;
			opt.Zero_separator = 1;
			continue;
		}

		usage(EINVAL);
	}
}

typedef struct { char path[4096]; } path;

UHASH_DEFINE_TYPE0(uh1, ino_t);
typedef struct { uh1 dir, file; } seen_t;
UHASH_DEFINE_TYPE2(uh0, dev_t, seen_t);

UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(dev_t);
UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(ino_t);

static seen_t  empty_seen;
static uh0     devroot;

static void dump_error(int error_num, const char * where);
static void dump_path_error(int error_num, const char * where, const char * name);

static void process_file(dev_t dev, ino_t ino, char * name, uint32_t name_len);
static void process_dir(dev_t dev, ino_t ino, char * name, uint32_t name_len);

static int handle_file_type(uint32_t type, const char * arg, const char * dir, uint32_t dir_len);
static uint32_t resolve_fd(int fd, char * buffer, uint32_t buffer_size);

static void process_arg(const char * name)
{
	int f_fd = open(name, O_RDONLY | O_PATH);
	if (f_fd < 0) {
		dump_path_error(errno, "process_arg:open(2)", name);
		return;
	}

	static struct stat f_stat;

	memset(&f_stat, 0, sizeof(f_stat));
	if (fstat(f_fd, &f_stat) < 0) {
		dump_path_error(errno, "process_arg:fstat(2)", name);
		goto process_arg__close;
	}

	if (!handle_file_type(IFTODT(f_stat.st_mode), name, NULL, 0)) {
		goto process_arg__close;
	}

	char tname[sizeof(path)];

	memset(tname, 0, sizeof(tname));
	uint32_t tname_len = resolve_fd(f_fd, tname, sizeof(tname));
	if (!tname_len) {
		goto process_arg__close;
	}

	close(f_fd); f_fd = -1;

	switch (f_stat.st_mode & S_IFMT) {
	case S_IFREG: process_file(f_stat.st_dev, f_stat.st_ino, tname, tname_len); break;
	case S_IFDIR: process_dir(f_stat.st_dev, f_stat.st_ino, tname, tname_len); break;
	}

process_arg__close:

	if (f_fd >= 0) close(f_fd);

	return;
}

static void process_file(dev_t dev, ino_t ino, char * name, uint32_t name_len)
{
	UHASH_IDX_T i_seen = UHASH_CALL(uh0, search, &devroot, dev);
	if (!i_seen)
		i_seen = UHASH_CALL(uh0, insert, &devroot, dev, &empty_seen);

	seen_t * p_seen = (seen_t *) UHASH_CALL(uh0, value, &devroot, i_seen);
	UHASH_IDX_T i_ino = UHASH_CALL(uh1, search, &(p_seen->file), ino);
	if (i_ino) return;

	UHASH_CALL(uh1, insert, &(p_seen->file), ino);

	// fputs(name, stdout);
	// fputc(entry_separator, stdout);
	name[name_len] = entry_separator;
	write(STDOUT_FILENO, name, name_len + 1);
	name[name_len] = 0;
}

static CC_FORCE_INLINE int filter_out_dots(const struct dirent * entry)
{
	/*
	if (strcmp(entry->d_name, ".") == 0)  return 0;
	if (strcmp(entry->d_name, "..") == 0) return 0;
	return 1;
	*/

	if (entry->d_name[0] != '.') return 1;
	if (entry->d_name[1] == 0)   return 0;
	if (entry->d_name[1] != '.') return 1;
	if (entry->d_name[2] == 0)   return 0;

	return 1;
}

static void process_dir(dev_t dev, ino_t ino, char * name, uint32_t name_len)
{
	UHASH_IDX_T i_seen = UHASH_CALL(uh0, search, &devroot, dev);
	if (!i_seen)
		i_seen = UHASH_CALL(uh0, insert, &devroot, dev, &empty_seen);

	seen_t * p_seen = (seen_t *) UHASH_CALL(uh0, value, &devroot, i_seen);
	UHASH_IDX_T i_ino = UHASH_CALL(uh1, search, &(p_seen->dir), ino);
	if (i_ino) return;

	UHASH_CALL(uh1, insert, &(p_seen->dir), ino);

	DIR * d = opendir(name);
	if (!d) {
		dump_path_error(errno, "process_dir:opendir(3)", name);
		return;
	}

	if (name[name_len - 1] == '/') name_len--;

	char tname[sizeof(path)];

	struct dirent * dent;
	while ((dent = readdir(d))) {
		if (!filter_out_dots(dent))
			continue;

		if (!handle_file_type(dent->d_type, dent->d_name, name, name_len))
			continue;

		uint32_t dname_len = strnlen(dent->d_name, sizeof(dent->d_name) / sizeof(dent->d_name[0]));
		uint32_t tname_len = name_len + 1 /* "/" */ + dname_len;
		if (tname_len >= sizeof(tname)) {
			name[name_len] = '/';
			dump_path_error(ENAMETOOLONG, name, dent->d_name);
			name[name_len] = 0;
			continue;
		}

		// snprintf(tname, sizeof(tname), "%s/%s", name, dent->d_name);
		memcpy(tname, name, name_len);
		tname[name_len] = '/';
		memcpy(&tname[name_len + 1], dent->d_name, dname_len);
		tname[name_len + 1 + dname_len] = 0;

		switch (dent->d_type) {
		case DT_REG: process_file(dev, dent->d_ino, tname, tname_len);
			break;
		case DT_DIR: process_dir(dev, dent->d_ino, tname, tname_len);
			break;
		case DT_LNK: process_arg(tname);
			break;
		}
	}

	closedir(d);
}

static int seen_t__ctor(seen_t * s)
{
	UHASH_CALL(uh1, init, &(s->dir));
	UHASH_SET_DEFAULT_KEY_COMPARATOR(&(s->dir), ino_t);

	UHASH_CALL(uh1, init, &(s->file));
	UHASH_SET_DEFAULT_KEY_COMPARATOR(&(s->file), ino_t);

	return 0;
}

static int seen_t__dtor(seen_t * s)
{
	UHASH_CALL(uh1, free, &(s->dir));
	UHASH_CALL(uh1, free, &(s->file));
	return 0;
}

static void prepare_internals(void)
{
	UHASH_CALL(uh0, init, &devroot);
	UHASH_SET_DEFAULT_KEY_COMPARATOR(&devroot, dev_t);
	UHASH_SET_VALUE_HANDLERS(&devroot, seen_t__ctor, seen_t__dtor);

	memset(&empty_seen, 0, sizeof(empty_seen));
}

static int handle_file_type(uint32_t type, const char * arg, const char * dir, uint32_t dir_len)
{
	const char * e_type = NULL;
	switch (type) {
	case DT_REG:  break;
	case DT_DIR:  break;
	case DT_LNK:  e_type = (dir)
	                     ? NULL
	                     : "symbolic link"; break;

	case DT_BLK:  e_type = "block device";       break;
	case DT_CHR:  e_type = "character device";   break;
	case DT_FIFO: e_type = "FIFO";               break;
	case DT_SOCK: e_type = "socket";             break;
	default:      e_type = "unknown entry type"; break;
	}

	if (!e_type) return 1;

	if (opt.Quiet) return 0;

	if (dir) {
		fprintf(stderr, "%.*s: can't handle <%s>, skipping %s\n", dir_len, dir, e_type, arg);
	} else {
		fprintf(stderr, "can't handle <%s>, skipping %s\n", e_type, arg);
	}
	return 0;
}

static uint32_t resolve_fd(int fd, char * buffer, uint32_t buffer_size)
{
	static char proc_link[48];

	snprintf(proc_link, sizeof(proc_link), "/proc/self/fd/%d", fd);
	ssize_t result = readlink(proc_link, buffer, buffer_size - 1);
	if (result < 0) {
		dump_error(errno, "resolve_fd:readlink(2)");
		return 0;
	}
	return result;
}

static void dump_error(int error_num, const char * where)
{
	if (opt.Silent) return;

	char        * e_str = NULL;
	static char   e_buf[4096];

	memset(&e_buf, 0, sizeof(e_buf));
	e_str = strerror_r(error_num, e_buf, sizeof(e_buf) - 1);
	fprintf(stderr, "%s error %d: %s\n", where, error_num, e_str);
}

static void dump_path_error(int error_num, const char * where, const char * name)
{
	if (opt.Silent) return;

	char        * e_str = NULL;
	static char   e_buf[4096 + sizeof(path)];

	memset(&e_buf, 0, sizeof(e_buf));
	e_str = strerror_r(error_num, e_buf, sizeof(e_buf) - 1);
	fprintf(stderr, "%s path '%s' error %d: %s\n", where, name, error_num, e_str);
}
