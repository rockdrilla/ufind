/* ufind: simple find(1) replacement with destination uniqueness checks
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * (c) 2022, Konstantin Demin
 *
 * Rough alternative (but slow):
 *  for i ; do
 *      [ -n "$i" ] || continue
 *
 *      if [ -f "$i" ] ; then
 *          printf '%s\0' "$i"
 *      elif [ -d "$i" ] ; then
 *          find "$i/" -follow -type f -print0
 *      fi
 *  done \
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

static void detect_work_method(void);

static void prepare_internals(void);
static void propagate_args(int argc, char * argv[]);
static int has_pending_work(void);
static void process_pending_work(void);

static void process_arg(const char * arg);

static char entry_separator = '\n';
static int use_lists = 0;

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

	detect_work_method();
	prepare_internals();

	if (use_lists == 1) {
		propagate_args(argc, argv);
		while (has_pending_work() != 0) {
			process_pending_work();
		}
	} else {
		for (int i = 0; i < argc; i++) {
			process_arg(argv[i]);
		}
	}

	return 0;
}

static void detect_work_method(void)
{
	use_lists = 0;

	const char * env = getenv("UFIND_USE_LISTS");
	if (env == NULL)
		return;

	if (strlen(env) == 0)
		return;

	use_lists = 1;
}

typedef struct { char path[4096]; } path;

/* "work" lists */
ulist_t * work_current;
ulist_t * work_pending;

static void propagate_args(int argc, char * argv[])
{
	int i;
	ulist_idx_t k;
	for (i = 0; i < argc; i++) {
		k = ulist_append(work_pending, NULL);
		strcpy(ulist_get(work_pending, k), argv[i]);
	}
}

static int has_pending_work(void)
{
	return (work_pending->used != 0) ? 1 : 0;
}

static void process_item(const char * arg, ulist_idx_t index);

static void process_pending_work(void)
{
	work_current = work_pending;
	work_pending = malloc(sizeof(ulist_t));
	ulist_init(work_pending, sizeof(path));

	ulist_walk(work_current, (ulist_item_visitor) process_item);

	ulist_free(work_current);
	free(work_current);
	work_current = NULL;
}

static void process_item(const char * arg, ulist_idx_t index)
{
	process_arg(arg);
}

static void dump_error(int error_num, const char * where);
static void dump_path_error(int error_num, const char * where, const char * name);

static int handle_file_type(mode_t type, const char * arg);
static int handle_direntry_type(uint type, const char * arg);
static int resolve_fd(int fd, char * buffer, size_t buffer_size);

static void process_file(dev_t dev, ino_t ino, const char * name);
static void process_dir(dev_t dev, ino_t ino, const char * name);

static void process_arg(const char * arg)
{
	int f_fd = open(arg, O_RDONLY | O_PATH);
	if (f_fd < 0) {
		dump_path_error(errno, "process_arg:open(2)", arg);
		return;
	}

	struct stat f_stat;
	memset(&f_stat, 0, sizeof(f_stat));

	if (fstat(f_fd, &f_stat) < 0) {
		dump_path_error(errno, "process_arg:fstat(2)", arg);
		goto err_close_file;
	}

	if (handle_file_type(f_stat.st_mode, arg) == 0) {
		goto err_close_file;
	}

	char * name = calloc(1, sizeof(path));
	if (resolve_fd(f_fd, name, sizeof(path)) == 0) {
		goto err_free_buffers;
	}

	close(f_fd);

	switch (f_stat.st_mode & S_IFMT) {
	case S_IFREG: process_file(f_stat.st_dev, f_stat.st_ino, name) ; break;
	case S_IFDIR: process_dir(f_stat.st_dev, f_stat.st_ino, name) ; break;
	}

	free(name);

	return;

err_free_buffers:

	free(name);

err_close_file:

	close(f_fd);
	return;
}

/* "visited" entries */
UHASH_DEFINE__TYPE0(uh_ino, ino_t);
UHASH_DEFINE__TYPE2(uh_dev_ino, dev_t, uh_ino);

static uh_dev_ino filenames;
static uh_dev_ino visited_dirs;

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

	fputs(name, stdout);
	fputc(entry_separator, stdout);
}

static int filter_out_dots(const struct dirent * entry)
{
	if (strcmp(entry->d_name, ".") == 0)
		return 0;
	if (strcmp(entry->d_name, "..") == 0)
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

	struct dirent ** e_list;
	int e_count = scandir(name, &e_list, filter_out_dots, NULL);
	if (e_count < 0) {
		dump_path_error(errno, "process_dir:scandir(3)", name);
		return;
	}

	UHASH_CALL(uh_ino, insert, h_ino, ino);

	char * tmpname = malloc(sizeof(path));

	for (int i = 0; i < e_count; i++) {
		memset(tmpname, 0, sizeof(path));
		snprintf(tmpname, sizeof(path) - 1, "%s/%s", name, e_list[i]->d_name);

		if (handle_direntry_type(e_list[i]->d_type, tmpname) == 0) {
			goto free_list_entry;
		}

		if (use_lists == 1) {
			ulist_append(work_pending, tmpname);
		} else {
			process_arg(tmpname);
		}

	free_list_entry:
		free(e_list[i]);
	}
	free(e_list);

	free(tmpname);
}

UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(dev_t);
UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(ino_t);

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
	if (use_lists == 1) {
		work_pending = malloc(sizeof(ulist_t));
		ulist_init(work_pending, sizeof(path));
	}

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

static int handle_direntry_type(uint type, const char * arg)
{
	const char * e_type = NULL;
	switch (type) {
	case DT_REG:   break;
	case DT_DIR:   break;
	case DT_LNK:   break;
	case DT_BLK:   e_type = "block device"; break;
	case DT_CHR:   e_type = "character device"; break;
	case DT_FIFO:  e_type = "FIFO"; break;
	case DT_SOCK:  e_type = "socket"; break;
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
	static char   e_buf[4096];

	memset(&e_buf, 0, sizeof(e_buf));
	e_str = strerror_r(error_num, e_buf, sizeof(e_buf) - 1);
	fprintf(stderr, "%s error %d: %s\n", where, error_num, e_str);
}

static void dump_path_error(int error_num, const char * where, const char * name)
{
	char        * e_str = NULL;
	static char   e_buf[4096];

	memset(&e_buf, 0, sizeof(e_buf));
	e_str = strerror_r(error_num, e_buf, sizeof(e_buf) - 1);
	fprintf(stderr, "%s path '%s' error %d: %s\n", where, name, error_num, e_str);
}
