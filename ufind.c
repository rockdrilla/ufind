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

#include "include/io/const.h"
#include "include/io/log-stderr.h"
#include "include/procfs/fd2name.h"
#include "include/uhash/uhash.h"

#define UFIND_OPTS "hqvxz"

static void usage(int retcode)
{
	fputs(
	"ufind 0.4.5\n"
	"Usage: ufind [-" UFIND_OPTS "] <path> [..<path>]\n"
	" -h  - help (show this message)\n"
	" -q  - quiet (don't print error messages in stderr)\n"
	" -v  - verbose (print information messages in stderr)\n"
	" -x  - [no] cross dev/fs (skip entries on different file systems)\n"
	" -z  - zero-separator (separate entries with \\0 instead of \\n)\n"
	, stderr);

	exit(retcode);
}

static struct {
	uint8_t
	  Quiet
	, Verbose
	, Xdev
	, Zero_separator
	;
} opt;

static char entry_separator = '\n';
static uint8_t devroot_seal = 0;

static void parse_opts(int argc, char * argv[]);
static void prepare(void);
static void process_arg(const char * arg);
static void debug_print_devroot(void);

int main(int argc, char * argv[])
{
	if (argc < 2) usage(0);

	parse_opts(argc, argv);
	prepare();

	for (int i = optind; i < argc; i++) {
		devroot_seal = 0;
		process_arg(argv[i]);
	}

	if (opt.Verbose > 1) debug_print_devroot();

	return 0;
}

static void parse_opts(int argc, char * argv[])
{
	memset(&opt, 0, sizeof(opt));

	int o;
	while ((o = getopt(argc, (char * const *) argv, UFIND_OPTS)) != -1) {
		switch (o) {
		case 'h':
			usage(0);
			break;
		case 'q':
			if (opt.Quiet > 3) break;
			opt.Quiet++;
			continue;
		case 'v':
			if (opt.Verbose > 3) break;
			opt.Verbose++;
			continue;
		case 'x':
			if (opt.Xdev) break;
			opt.Xdev = 1;
			continue;
		case 'z':
			if (opt.Zero_separator) break;
			opt.Zero_separator = 1;
			continue;
		}

		usage(EINVAL);
	}

	if (optind >= argc) usage(EINVAL);
	if (opt.Zero_separator) entry_separator = 0;
}

UHASH_DEFINE_TYPE0(uh1, ino_t)
typedef struct { uh1 dir, file; } seen_t;
UHASH_DEFINE_TYPE2(uh0, dev_t, seen_t)

UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(dev_t)
UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(ino_t)

static seen_t  empty_seen;
static uh0     devroot;

static void dump_error(int error_num, const char * where);
static void dump_path_error(int error_num, const char * where, const char * name);

static void process_file(dev_t dev, ino_t ino, char * name, uint32_t name_len);
static void process_dir(dev_t dev, ino_t ino, char * name, uint32_t name_len);

static int handle_file_type(uint32_t type, const char * arg, const char * dir, uint32_t dir_len);

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

	char tname[PATH_MAX];

	uint32_t tname_len = procfs_fd2name(f_fd, tname, sizeof(tname));
	if (!tname_len) {
		dump_error(errno, "process_arg:readlink(2)");
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

static inline seen_t * process_dev(dev_t dev, const char * name, uint32_t name_len)
{
	UHASH_IDX_T i_seen = 0;
	while (1) {
		if (!devroot_seal) break;

		i_seen = UHASH_CALL(uh0, search, &devroot, dev);
		if (i_seen) break;

		if (opt.Verbose)
			fprintf(stderr, "filesystem boundary violation: %.*s\n", name_len, name);
		return NULL;
	}

	devroot_seal = opt.Xdev;
	if (!i_seen) i_seen = UHASH_CALL(uh0, search, &devroot, dev);
	if (!i_seen) i_seen = UHASH_CALL(uh0, insert, &devroot, dev, &empty_seen);

	if (!i_seen) {
		if (!opt.Quiet)
			fprintf(stderr, "error while inserting dev_t %lu for: %.*s\n", dev, name_len, name);
		return NULL;
	}

	return (seen_t *) UHASH_CALL(uh0, value, &devroot, i_seen);
}

static void process_file(dev_t dev, ino_t ino, char * name, uint32_t name_len)
{
	seen_t * p_seen = process_dev(dev, name, name_len);
	if (!p_seen) return;
	UHASH_IDX_T i_ino = UHASH_CALL(uh1, insert_strict, &(p_seen->file), ino);
	if (!i_ino) return;

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
	seen_t * p_seen = process_dev(dev, name, name_len);
	if (!p_seen) return;
	UHASH_IDX_T i_ino = UHASH_CALL(uh1, insert_strict, &(p_seen->dir), ino);
	if (!i_ino) return;

	DIR * d = opendir(name);
	if (!d) {
		dump_path_error(errno, "process_dir:opendir(3)", name);
		return;
	}

	if (name[name_len - 1] != '/') {
		name[name_len] = '/';
		name_len++;
	}

	char tname[PATH_MAX];

	struct dirent * dent;
	uint32_t dname_len, tname_len;
	while ((dent = readdir(d))) {
		if (!filter_out_dots(dent))
			continue;

		if (!handle_file_type(dent->d_type, dent->d_name, name, name_len))
			continue;

		dname_len = strnlen(dent->d_name, sizeof(dent->d_name) / sizeof(dent->d_name[0]));
		tname_len = name_len + dname_len;
		if (tname_len >= sizeof(tname)) {
			dump_path_error(ENAMETOOLONG, name, dent->d_name);
			continue;
		}

		memcpy(tname, name, name_len);
		memcpy(&(tname[name_len]), dent->d_name, dname_len);
		tname[name_len + dname_len] = 0;

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

static void prepare(void)
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

	if (!opt.Verbose) return 0;

	if (dir) {
		fprintf(stderr, "%.*s: won't handle <%s>, skipping %s\n", dir_len, dir, e_type, arg);
	} else {
		fprintf(stderr, "won't handle <%s>, skipping %s\n", e_type, arg);
	}
	return 0;
}

static void dump_error(int error_num, const char * where)
{
	if (opt.Quiet) return;

	log_stderr_error_ex("ufind:", error_num, "%s", where);
}

static void dump_path_error(int error_num, const char * where, const char * name)
{
	if (opt.Quiet) return;

	log_stderr_path_error_ex("ufind:", name, error_num, "%s", where);
}

static void debug_print_uh0_node(unsigned int index, const uhash_uh0_node * v, void * state)
{
	const uh0 * h = (const uh0 *) state;
	const seen_t * s = UHASH_CALL(uh0, value, h, v->value);

	index += 1;

	if (s) {
		fprintf(stderr, "#  %c [%u]: Key %lu File: %u/%u Dir: %u/%u\n",
		                (h->tree_root == index) ? '>' : ' ', index, v->key,
		                s->file.nodes.used, s->file.nodes.allocated,
		                s->dir.nodes.used, s->dir.nodes.allocated);
	} else {
		fprintf(stderr, "#  %c [%u]: Key %lu\n <no data>\n",
		                (h->tree_root == index) ? '>' : ' ', index, v->key);
	}
}

static void debug_print_devroot(void)
{
	fprintf(stderr, "# devroot:\n");
	fprintf(stderr, "#  Nodes: %u/%u Values: %u/%u\n",
	                devroot.nodes.used, devroot.nodes.allocated,
	                devroot.values.used, devroot.values.allocated);
	fprintf(stderr, "#  Items: [\n");
	UHASH_VCALL(uh0, v_node, const_walk_ex, &(devroot.nodes), debug_print_uh0_node, (void *) &devroot);
	fprintf(stderr, "#  ]\n");
}
