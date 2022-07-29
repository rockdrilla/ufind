## About

`ufind` enumerates unique files and output their names to `stdout`.

`ufind` is small supplemental utility for [minimal Debian container image](https://github.com/rockdrilla/docker-debian) (*work in progress*) and mostly useful for building container images (`RUN <...>` stanzas in `Dockerfile`/`Containerfile`).

Program is kinda *dumb* so feel free to open issue/PR. :)

---

## How it works:

`ufind` treats `dev_t:ino_t` as "unique identifier" for files and directories.

`ufind` iterates through each argument:

- argument is symlink:
  - determine underlying file system object type with `open(2)`/`fstat(2)`
  - call appropriate handler

> `ufind` handles command line arguments as they were symlinks.

- argument is neither regular file nor directory (e.g. devices, sockets, etc.):
  - print warning to `stderr`

- argument is regular file:
  - it's identifier is seen first time:
    - remember identifier
    - print (full) file name to `stdout`
  - it's identifier is already known:
    - do nothing (file is already handled)

- argument is directory:
  - it's identifier is seen first time:
    - remember identifier
    - iterate it's members and handle them accordingly
  - it's identifier is already known:
    - do nothing (directory is already handled)

Nota bene: there's no sorting of any kind (directory entries or output)
so consecutive calls against same file system MAY produce different output.

### Example:

Consider following tree:

```sh
  $ find * | sort -V | xargs -r stat -c '%A %d:%i %N'

  drwxr-xr-x 36:31 'dir1'
  -rw-r--r-- 36:34 'dir1/file1'
  lrwxrwxrwx 36:41 'dir1/link1' -> 'file1'
  lrwxrwxrwx 36:44 'dir1/link2' -> '../dir2/dir3/file2'
  drwxr-xr-x 36:32 'dir2'
  drwxr-xr-x 36:33 'dir2/dir3'
  -rw-r--r-- 36:36 'dir2/dir3/file2'
  -rw-r--r-- 36:34 'dir2/dir3/hardlink2'
  lrwxrwxrwx 36:28 'dir2/dir3/link3' -> '../file3'
  -rw-r--r-- 36:27 'dir2/file3'
  -rw-r--r-- 36:27 'dir2/hardlink1'
```

`ufind` produces following output:

```sh
  $ ufind *

  /tmp/ufind-example/dir1/file1
  /tmp/ufind-example/dir2/hardlink1
  /tmp/ufind-example/dir2/dir3/file2

  $ ufind * | sort -V | xargs -r stat -c '%A %d:%i %N'

  -rw-r--r-- 36:34 '/tmp/ufind-example/dir1/file1'
  -rw-r--r-- 36:36 '/tmp/ufind-example/dir2/dir3/file2'
  -rw-r--r-- 36:27 '/tmp/ufind-example/dir2/hardlink1'
```

As seen, output MAY be totally unsorted and a bit confusing:

- `'dir2/hardlink1'` was printed instead of `'dir2/file3'` because it has "arrived" first (both files are "hardlinks");
- `'dir2/dir3/file2'` was printed after `'dir2/hardlink1'`.

## Usage:

`ufind [-z] <path> [..<path>]`

### Options:

| Option | Description                            |
| ------ | -------------------------------------- |
|  `-z`  | separate entries with \0 instead of \n |

## Pros:

- beats shell script variant by speed (see below)
- much smaller than [sharkdp/fd](https://github.com/sharkdp/fd) :)

## Cons:

- error-prone to TOCTOU, that's why it's safely usable only while building container images
- much slower than [sharkdp/fd](https://github.com/sharkdp/fd) :(

## Benchmark:

```sh
$ ./benchmark.sh

find /usr/share/: 283379
./ufind-terse.sh /usr/share/: 235382
./ufind /usr/share/: 235382
fdfind -u -j 1 . /usr/share/: 283378
fdfind -u . /usr/share/: 283378

Benchmark 1: ./ufind-terse.sh /usr/share
  Time (mean ± σ):      3.791 s ±  0.025 s    [User: 3.793 s, System: 0.655 s]
  Range (min … max):    3.757 s …  3.821 s    10 runs

Benchmark 1: ./ufind /usr/share
  Time (mean ± σ):     693.0 ms ±  28.9 ms    [User: 509.9 ms, System: 181.6 ms]
  Range (min … max):   664.0 ms … 738.1 ms    50 runs

Benchmark 1: fdfind -u -j 1 . /usr/share
  Time (mean ± σ):     245.6 ms ±   5.0 ms    [User: 212.2 ms, System: 174.1 ms]
  Range (min … max):   239.1 ms … 259.1 ms    50 runs

Benchmark 1: fdfind -u . /usr/share
  Time (mean ± σ):     120.0 ms ±   1.0 ms    [User: 535.8 ms, System: 1021.7 ms]
  Range (min … max):   116.9 ms … 121.5 ms    50 runs
```

## Building from source:

Only "standard" things are required: binutils, gcc and libc6-dev.

`gcc -o ufind ufind.c`

## Shell script alternative:

simple but slow (ref: [`ufind-terse.sh`](ufind-terse.sh))

```sh
#!/bin/sh
find "$@" -follow -type f -print0 \
| xargs -0 -r -n 64 stat -L --printf='%d:%i|%n\0' \
| sort -z -u -t '|' -k1,1 \
| cut -z -d '|' -f 2
```

---

## License

Apache-2.0

- [spdx.org](https://spdx.org/licenses/Apache-2.0.html)
- [opensource.org](https://opensource.org/licenses/Apache-2.0)
- [apache.org](https://www.apache.org/licenses/LICENSE-2.0)
